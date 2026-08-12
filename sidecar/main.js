// SPDX-FileCopyrightText: 2026 Miguel Rincon
// SPDX-FileCopyrightText: 2026 the Kadenza authors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Derived from Slipmat's sidecar. Electron is visible only while Apple's own
// sign-in UI needs interaction; MusicKit and Widevine remain hidden otherwise.

'use strict'

const { app, BrowserWindow, components, ipcMain, powerSaveBlocker, session, shell } = require('electron')
const path = require('node:path')
const readline = require('node:readline')

const APPLE_MUSIC = 'https://music.apple.com/'
const PARTITION = 'persist:kadenza'
const DEBUG = process.env.KADENZA_SHOW_SIDECAR === '1'
const READY_TIMEOUT_MS = 60000
const PROBE_INTERVAL_MS = 500
const TOKEN_NUDGES = 10

app.setName('Kadenza')
if (process.platform === 'linux') app.setDesktopName('io.github.timpalpant.kadenza.desktop')
app.commandLine.appendSwitch('disable-features', 'MediaSessionService,HardwareMediaKeyHandling')
app.commandLine.appendSwitch('disk-cache-size', String(200 * 1024 * 1024))
app.commandLine.appendSwitch('autoplay-policy', 'no-user-gesture-required')
app.commandLine.appendSwitch('disable-gpu')
app.commandLine.appendSwitch('disable-renderer-backgrounding')
app.commandLine.appendSwitch('disable-background-timer-throttling')
app.commandLine.appendSwitch('disable-backgrounding-occluded-windows')

let win = null
let hookReady = false
let pending = []
let probeTimer = null
let tokenTimer = null
let blocker = null
// Set once the user asks to sign in, so a silent restore can never steal or
// hide the window Apple opens for them.
let signInRequested = false
let sessionSettled = false
const authWindows = new Set()

// Both pipes belong to Kadenza, so both break the moment it goes away. Node
// raises EPIPE on the next write, and an uncaught one in Electron's main
// process puts a modal "A JavaScript error occurred" dialog on screen — from a
// helper the user cannot see and about a parent that no longer exists.
function send(message) {
  try {
    process.stdout.write(JSON.stringify(message) + '\n')
  } catch { /* Kadenza is gone; the shutdown path below handles it */ }
}

function log(...parts) {
  try {
    process.stderr.write('[kadenza-sidecar] ' + parts.join(' ') + '\n')
  } catch { /* nowhere left to log to */ }
}

function fail(code, error) {
  const detail = String((error && error.message) || error || '')
  log('ERROR', code, detail)
  send({ event: 'error', code, detail })
}

function allowed(url) {
  try {
    const hostname = new URL(url).hostname
    return hostname === 'apple.com' || hostname.endsWith('.apple.com') || hostname.endsWith('.mzstatic.com')
  } catch {
    return false
  }
}

function conceal() {
  if (!win || win.isDestroyed()) return
  if (blocker === null) blocker = powerSaveBlocker.start('prevent-app-suspension')
  win.hide()
  win.setSkipTaskbar(true)
}

function reveal() {
  if (!win || win.isDestroyed()) return
  win.setSkipTaskbar(false)
  win.setSize(1000, 720)
  win.center()
  win.show()
  win.focus()
}

async function createWindow() {
  win = new BrowserWindow({
    show: DEBUG,
    width: 1000,
    height: 720,
    title: 'Sign in to Apple Music — Kadenza',
    autoHideMenuBar: true,
    skipTaskbar: !DEBUG,
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      partition: PARTITION,
      contextIsolation: false,
      nodeIntegration: false,
      sandbox: false,
      backgroundThrottling: false,
    },
  })

  win.webContents.on('will-navigate', (event, url) => {
    if (!allowed(url)) {
      event.preventDefault()
      shell.openExternal(url)
    }
  })
  win.webContents.setWindowOpenHandler(({ url }) => {
    if (allowed(url)) {
      return {
        action: 'allow',
        overrideBrowserWindowOptions: {
          show: true,
          width: 520,
          height: 720,
          title: 'Sign in to Apple Music — Kadenza',
          autoHideMenuBar: true,
          webPreferences: { partition: PARTITION },
        },
      }
    }
    shell.openExternal(url)
    return { action: 'deny' }
  })
  win.webContents.on('did-create-window', child => {
    authWindows.add(child)
    child.setMenuBarVisibility(false)
    child.setTitle('Sign in to Apple Music — Kadenza')
    child.webContents.on('will-navigate', (event, url) => {
      if (!allowed(url)) {
        event.preventDefault()
        shell.openExternal(url)
      }
    })
    child.webContents.setWindowOpenHandler(({ url }) => {
      if (!allowed(url)) shell.openExternal(url)
      return { action: 'deny' }
    })
    child.on('closed', () => authWindows.delete(child))
  })
  win.webContents.session.setPermissionRequestHandler((_contents, permission, callback) => {
    callback(permission === 'media')
  })
  win.webContents.on('did-start-navigation', (...args) => {
    const first = args[0]
    const details = first && typeof first === 'object' && 'isMainFrame' in first
      ? first : { isMainFrame: args[3], isSameDocument: args[2] }
    if (details.isMainFrame && !details.isSameDocument) {
      hookReady = false
      probeForMusicKit()
    }
  })
  win.on('close', event => {
    event.preventDefault()
    conceal()
    send({ event: 'window-hidden' })
  })

  await restoreSessionCookie()
  await win.loadURL(APPLE_MUSIC)
  if (!DEBUG) conceal()
  probeForMusicKit()
}

// Apple's web player reads its session from a media-user-token cookie on
// music.apple.com. Signing in happens on authorize.music.apple.com, which sets
// that cookie on its own host and on mediaauth.apple.com but never on the host
// the player actually reads. So a profile that had signed in still booted
// signed out, and MusicKit stayed unauthorized — which is also why playback
// fell back to previews. Copy the token onto the player's own domain before
// the page loads. Nothing is written outside Electron's profile.
async function restoreSessionCookie() {
  const jar = session.fromPartition(PARTITION)
  let saved = []
  try {
    saved = await jar.cookies.get({ name: 'media-user-token' })
  } catch (error) {
    return log('could not read the saved Apple session:', error && error.message)
  }
  const onPlayerHost = cookie => {
    const domain = cookie.domain || ''
    return domain === 'music.apple.com' || domain === '.music.apple.com'
  }
  if (saved.some(cookie => onPlayerHost(cookie) && cookie.value)) {
    return send({ event: 'session-cookie', detail: 'already present' })
  }
  // Any host Apple left it on carries the same token; prefer the longest-lived.
  const source = saved
    .filter(cookie => cookie.value)
    .sort((a, b) => (b.expirationDate || 0) - (a.expirationDate || 0))[0]
  if (!source) {
    return send({ event: 'session-cookie', detail: 'none saved' })
  }
  try {
    await jar.cookies.set({
      url: 'https://music.apple.com/',
      name: 'media-user-token',
      value: source.value,
      domain: '.music.apple.com',
      path: '/',
      secure: true,
      httpOnly: false,
      // Apple's sign-in redirects across hosts, so the cookie has to survive
      // a cross-site navigation.
      sameSite: 'no_restriction',
      expirationDate: source.expirationDate ||
        Math.floor(Date.now() / 1000) + 60 * 60 * 24 * 180,
    })
    send({ event: 'session-cookie', detail: `restored from ${source.domain}` })
  } catch (error) {
    send({ event: 'session-cookie', detail: `failed: ${error && error.message}` })
  }
}

function probeForMusicKit() {
  clearInterval(probeTimer)
  clearInterval(tokenTimer)
  const deadline = Date.now() + READY_TIMEOUT_MS
  probeTimer = setInterval(async () => {
    if (!win || win.isDestroyed()) return clearInterval(probeTimer)
    if (Date.now() > deadline) {
      clearInterval(probeTimer)
      return send({ event: 'hook-failed', detail: 'MusicKit never appeared on music.apple.com' })
    }
    if (win.webContents.isLoadingMainFrame()) return
    try {
      const ready = await win.webContents.executeJavaScript(
        'window.__kadenzaReady ? window.__kadenzaReady() : false',
      )
      if (ready) {
        clearInterval(probeTimer)
        win.webContents.send('kadenza:wire')
      }
    } catch (error) {
      log('probe failed:', error && error.message)
    }
  }, PROBE_INTERVAL_MS)
}

async function signOut() {
  if (win && !win.isDestroyed()) win.webContents.send('kadenza:command', { cmd: 'unauthorize' })
  try {
    await session.fromPartition(PARTITION).clearStorageData({
      storages: ['cookies', 'localstorage', 'sessionstorage', 'indexdb', 'websql', 'serviceworkers', 'cachestorage'],
    })
  } catch (error) {
    return fail('sign-out-failed', error)
  }
  hookReady = false
  signInRequested = false
  sessionSettled = false
  if (win && !win.isDestroyed()) {
    await win.loadURL(APPLE_MUSIC)
    conceal()
    probeForMusicKit()
  }
  send({ event: 'signed-out' })
}

function dispatch(message) {
  switch (message.cmd) {
  case 'showLogin':
    signInRequested = true
    clearInterval(tokenTimer)
    for (const child of authWindows) {
      if (!child.isDestroyed()) child.close()
    }
    authWindows.clear()
    conceal()
    return
  case 'hide': conceal(); return
  case 'signOut': void signOut(); return
  case 'quit': app.exit(0); return
  }
  if (message.cmd === 'authorize') {
    signInRequested = true
    clearInterval(tokenTimer)
  }
  if (!hookReady) {
    pending.push(message)
    send({ event: 'cmd-queued', cmd: message.cmd, depth: pending.length })
    return
  }
  win.webContents.send('kadenza:command', message)
}

// MusicKit rehydrates a previously authorized session from the persisted
// partition, but not always by the time the hook attaches, so poll briefly.
// Deliberately not gated on a named cookie: Apple may keep the token in web
// storage instead, and a missing cookie should not mean "sign in again".
// restoreAuthorization never calls authorize(), so this can never surface
// Apple's UI on its own.
function beginSessionRestore() {
  if (signInRequested || sessionSettled || !win || win.isDestroyed()) return
  clearInterval(tokenTimer)
  let attempts = 0
  tokenTimer = setInterval(() => {
    if (!win || win.isDestroyed() || sessionSettled || signInRequested) {
      return clearInterval(tokenTimer)
    }
    if (++attempts > TOKEN_NUDGES) {
      clearInterval(tokenTimer)
      return void reauthorizeSavedSession()
    }
    win.webContents.send('kadenza:command', { cmd: 'restoreAuthorization' })
  }, 1000)
}

// MusicKit had nothing to adopt. Apple keeps media-user-token on its
// authorize/mediaauth hosts, which the music.apple.com page cannot read, so a
// profile that has signed in before still looks signed out to MusicKit.
// Re-running authorize() lets Apple exchange those cookies for a token: it
// completes without interaction while the saved session is valid, and shows
// Apple's own window if it is not. Skipped entirely when nothing was ever
// signed in, so a first run goes straight to Kadenza's sign-in page.
async function reauthorizeSavedSession() {
  if (signInRequested || sessionSettled || !win || win.isDestroyed()) return
  let previouslySignedIn = false
  try {
    const saved = await session.fromPartition(PARTITION).cookies.get({
      name: 'media-user-token',
    })
    previouslySignedIn = saved.length > 0
  } catch (error) {
    log('could not inspect saved Apple session:', error && error.message)
  }
  if (!previouslySignedIn || signInRequested || sessionSettled) {
    return send({ event: 'session-restore-failed', detail: 'No saved Apple Music session' })
  }
  send({ event: 'session-reauthorizing' })
  win.webContents.send('kadenza:command', { cmd: 'authorize' })
}

function drainPending() {
  const queued = pending
  pending = []
  for (const message of queued) win.webContents.send('kadenza:command', message)
}

app.whenReady().then(async () => {
  try {
    await components.whenReady()
    send({ event: 'widevine-ready' })
  } catch (error) {
    fail('widevine-unavailable', error)
    return app.exit(1)
  }

  ipcMain.on('kadenza:event', (_event, message) => {
    if (message && message.event === 'hook-ready') {
      hookReady = true
      drainPending()
      beginSessionRestore()
    }
    if (message &&
        ((message.event === 'tokens' && message.authorized) ||
         (message.event === 'authorization' && message.authorized))) {
      // Make successful sign-in durable before the popup disappears or Kadenza
      // is closed. The partition also persists web storage automatically.
      sessionSettled = true
      clearInterval(tokenTimer)
      session.fromPartition(PARTITION).cookies.flushStore().catch(error => {
        log('could not flush Apple session cookies:', error && error.message)
      })
      conceal()
    }
    // Do not close Apple's child at the first token event: authorize() has not
    // necessarily finished establishing full-track playback at that point.
    // Interrupting it can leave MusicKit in preview-only mode (about 30 s).
    if (message &&
        ((message.event === 'cmd-done' && message.cmd === 'authorize') ||
         message.event === 'authorization-settled')) {
      sessionSettled = true
      clearInterval(tokenTimer)
      for (const child of authWindows) {
        if (!child.isDestroyed()) child.close()
      }
      authWindows.clear()
      conceal()
    }

    if (message && message.event === 'error' &&
        message.code === 'command-authorize' && !signInRequested) {
      send({ event: 'session-restore-failed', detail: 'Saved Apple Music session expired' })
    }
    send(message)
  })

  await createWindow()
  const input = readline.createInterface({ input: process.stdin })
  input.on('line', line => {
    if (!line.trim()) return
    try {
      dispatch(JSON.parse(line))
    } catch (error) {
      fail('bad-command', error)
    }
  })
  // The sidecar has no window of its own to close and no user to quit it, so
  // without this it outlives Kadenza: a crash, a SIGKILL or a session ending
  // leaves an invisible Electron holding ~180MB and the profile lock, and the
  // next launch contends with it. Kadenza closing its end of the pipe is the one
  // signal that arrives however the parent went away.
  input.on('close', () => {
    log('Kadenza closed the command pipe; shutting down')
    app.exit(0)
  })
  process.stdin.on('error', error => {
    log('command pipe failed:', error && error.message)
    app.exit(0)
  })
  send({ event: 'ready', debug: DEBUG })
}).catch(error => fail('startup-failed', error))

// terminate() before the parent resorts to kill(). Exiting here rather than
// letting the default handler run means Chromium tears its own children down
// instead of leaving them orphaned.
// A stream error is delivered to its 'error' listener rather than thrown, but
// only if there is one; without these Node throws instead.
process.stdout.on('error', () => app.exit(0))
process.stderr.on('error', () => {})

// Last resort. Anything unhandled here would otherwise raise Electron's modal
// error dialog over whatever the user is doing, so it goes to Kadenza's log and
// the sidecar stops; Kadenza restarts it.
process.on('uncaughtException', error => {
  log('unhandled error in the sidecar:', (error && error.stack) || error)
  app.exit(1)
})

for (const signal of ['SIGTERM', 'SIGINT', 'SIGHUP']) {
  process.on(signal, () => {
    log(`received ${signal}; shutting down`)
    app.exit(0)
  })
}

app.on('window-all-closed', () => {})
