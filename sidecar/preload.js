// SPDX-FileCopyrightText: 2026 Miguel Rincon
// SPDX-FileCopyrightText: 2026 the Kanzi authors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// MusicKit bridge derived from Slipmat. It never reads Apple page DOM or user
// credentials; its only integration surface is MusicKit's JavaScript object.

'use strict'

const { ipcRenderer } = require('electron')
const READY_TIMEOUT_MS = 60000
const READY_POLL_MS = 250
let music = null
let lastTokens = ''
let authorizedUserToken = null
let authorizationSettled = false

const emit = (event, payload = {}) => ipcRenderer.send('kanzi:event', { event, ...payload })

function pick(...getters) {
  for (const getter of getters) {
    try {
      const value = getter()
      if (value !== undefined && value !== null && value !== '') return value
    } catch { /* feature detection */ }
  }
  return null
}

function getInstance() {
  return pick(() => window.MusicKit && window.MusicKit.getInstance())
}

window.__kanziReady = () => !!getInstance()

function readTokens() {
  if (!music) return null
  const developerToken = pick(
    () => music.developerToken,
    () => music.api && music.api.developerToken,
    () => music._api && music._api.developerToken,
    () => window.MusicKit._instance && window.MusicKit._instance.developerToken,
  )
  if (!developerToken) return null
  const musicUserToken = pick(
    () => music.musicUserToken,
    () => music.api && music.api.userToken,
    () => authorizedUserToken,
  )
  return {
    developerToken,
    musicUserToken,
    storefront: pick(() => music.storefrontId, () => music.storefrontCountryCode) || 'us',
    // The token returned by authorize() is authoritative. isAuthorized can
    // briefly fall back to false while Apple's web player rehydrates itself.
    authorized: !!musicUserToken || !!pick(() => music.isAuthorized),
  }
}

function pushTokens() {
  const tokens = readTokens()
  if (!tokens) return null
  const fingerprint = JSON.stringify(tokens)
  if (fingerprint !== lastTokens) {
    lastTokens = fingerprint
    emit('tokens', tokens)
  }
  return tokens
}

const states = ['none', 'loading', 'playing', 'paused', 'stopped', 'ended', 'seeking', 'unknown', 'waiting', 'stalled', 'completed']
const stateName = state => states[state] || 'unknown'

function serializeItem(item) {
  if (!item) return null
  return {
    id: pick(() => item.id, () => item.playbackId),
    catalogId: pick(() => item.catalogId, () => item.container && item.container.id),
    title: pick(() => item.title, () => item.attributes && item.attributes.name) || '',
    artist: pick(() => item.artistName, () => item.attributes && item.attributes.artistName) || '',
    album: pick(() => item.albumName, () => item.attributes && item.attributes.albumName) || '',
    durationMs: pick(() => item.playbackDuration, () => item.attributes && item.attributes.durationInMillis) || 0,
    catalogDurationMs: pick(() => item.attributes && item.attributes.durationInMillis,
      () => item.playbackDuration) || 0,
    artistId: pick(() => item.relationships && item.relationships.artists &&
      item.relationships.artists.data && item.relationships.artists.data[0] &&
      item.relationships.artists.data[0].id),
    albumId: pick(() => item.relationships && item.relationships.albums &&
      item.relationships.albums.data && item.relationships.albums.data[0] &&
      item.relationships.albums.data[0].id),
    artworkTemplate: pick(() => item.artwork && item.artwork.url,
      () => item.attributes && item.attributes.artwork && item.attributes.artwork.url),
  }
}

function queuePayload() {
  const items = pick(() => music.queue && music.queue.items) || []
  return {
    position: pick(() => music.queue && music.queue.position) ?? -1,
    items: items.map(serializeItem),
  }
}

function on(name, handler) {
  try {
    music.addEventListener(name, handler)
  } catch {
    emit('hook-warning', { detail: `MusicKit event unavailable: ${name}` })
  }
}

// music.apple.com boots its player in preview mode for signed-out visitors.
// The property is missing from some MusicKit builds and read-only in others,
// where assigning to it throws under strict mode — neither may break playback.
// Returns what actually stuck, so a preview can be explained rather than just
// observed.
function disablePreviewOnly() {
  if (!music || !('previewOnly' in music)) return { supported: false, value: null }
  let error = null
  try {
    music.previewOnly = false
  } catch (caught) {
    error = (caught && caught.message) || String(caught)
  }
  return { supported: true, value: pick(() => music.previewOnly), error }
}

// Emitted when a track starts so a preview can be traced to its cause:
// whether MusicKit was still preview-locked, unauthorized, or without a
// subscription. Carries no credentials, only whether a token is present.
function emitPlaybackDiagnostics(trigger) {
  const previewOnly = disablePreviewOnly()
  const item = pick(() => music.nowPlayingItem)
  emit('playbackDiagnostics', {
    trigger,
    previewOnlySupported: previewOnly.supported,
    previewOnly: previewOnly.value,
    previewOnlyError: previewOnly.error || '',
    authorized: !!pick(() => music.isAuthorized),
    hasUserToken: !!pick(() => music.musicUserToken, () => authorizedUserToken),
    subscriptionStatus: String(
      pick(() => music.subscribeStatus, () => music.subscriptionStatus) ?? 'unknown'),
    catalogDurationMs: pick(() => item && item.attributes && item.attributes.durationInMillis) || 0,
    playbackDurationMs: Math.round((pick(() => music.currentPlaybackDuration) || 0) * 1000),
  })
}

function emitModes() {
  emit('modes', {
    shuffle: (pick(() => music.shuffleMode) ?? 0) === 1,
    repeat: ['none', 'one', 'all'][pick(() => music.repeatMode) ?? 0] || 'none',
  })
}

function wireEvents() {
  on('playbackStateDidChange', () => emit('playbackState', { state: stateName(pick(() => music.playbackState) ?? 0) }))
  on('nowPlayingItemDidChange', () => {
    emit('nowPlaying', {
      item: serializeItem(pick(() => music.nowPlayingItem)), queue: queuePayload(),
    })
    emitPlaybackDiagnostics('nowPlayingItemDidChange')
  })
  on('playbackTimeDidChange', () => emit('position', {
    positionMs: Math.round((pick(() => music.currentPlaybackTime) || 0) * 1000),
    durationMs: Math.round((pick(() => music.currentPlaybackDuration) || 0) * 1000),
  }))
  on('queueItemsDidChange', () => emit('queue', queuePayload()))
  on('queuePositionDidChange', () => emit('queue', queuePayload()))
  on('shuffleModeDidChange', emitModes)
  on('repeatModeDidChange', emitModes)
  on('playbackVolumeDidChange', () => emit('volume', { volume: pick(() => music.volume) ?? 1 }))
  on('authorizationStatusDidChange', () => {
    const tokens = pushTokens()
    emit('authorization', { authorized: !!(tokens && tokens.authorized) })
  })
}

async function enqueue(method, songs) {
  if (typeof music[method] !== 'function') throw new Error(`MusicKit has no ${method}`)
  await music[method]({ songs })
  emit('queue', queuePayload())
}

async function accepted(operation) {
  try {
    return await operation()
  } catch (error) {
    const empty = !error || error.body === undefined || error.body === null || error.body === ''
    if (error instanceof SyntaxError && empty) return null
    throw error
  }
}

async function libraryWrite(kind, id, operation) {
  try {
    await accepted(operation)
    emit('library-write', { kind, id, ok: true, detail: '' })
  } catch (error) {
    emit('library-write', {
      kind, id, ok: false, detail: String((error && error.message) || error),
    })
  }
}

function lyricTime(value) {
  if (!value) return 0
  if (value.endsWith('ms')) return Number.parseFloat(value) || 0
  if (value.endsWith('s')) return (Number.parseFloat(value) || 0) * 1000
  const parts = value.split(':').map(Number)
  if (parts.some(Number.isNaN)) return 0
  let seconds = 0
  for (const part of parts) seconds = seconds * 60 + part
  return Math.round(seconds * 1000)
}

function parseLyrics(value) {
  if (!value) return []
  if (Array.isArray(value)) return value
  if (typeof value !== 'string') return []
  if (!value.includes('<')) {
    return value.split(/\r?\n/).filter(Boolean).map(text => ({ timeMs: 0, text }))
  }
  const document = new DOMParser().parseFromString(value, 'application/xml')
  return Array.from(document.querySelectorAll('p')).map(node => ({
    timeMs: lyricTime(node.getAttribute('begin') || ''),
    text: (node.textContent || '').trim(),
  })).filter(line => line.text)
}

async function fetchLyrics(id) {
  try {
    const item = pick(() => music.nowPlayingItem)
    let source = pick(
      () => item.relationships.lyrics.data[0].attributes.ttml,
      () => item.relationships.lyrics.data[0].attributes.text,
      () => item.attributes.ttml,
    )
    if (!source) {
      const storefront = pick(() => music.storefrontId, () => music.storefrontCountryCode) || 'us'
      const path = `/v1/catalog/${encodeURIComponent(storefront)}/songs/${encodeURIComponent(id)}/lyrics`
      const response = typeof music.api.music === 'function'
        ? await music.api.music(path)
        : await music.api.get(path)
      // api.music() resolves to the whole HTTP response and puts the body in
      // .data, so the payload is response.data.data — one level deeper than
      // api.get(), which resolves to the body itself. Only the shallower shape
      // was handled, so every lyric lookup silently found nothing.
      source = pick(
        () => response.data.data[0].attributes.ttml,
        () => response.data.data[0].attributes.text,
        () => response.data[0].attributes.ttml,
        () => response.data[0].attributes.text,
        () => response.attributes.ttml,
        () => response.attributes.text,
      )
    }
    const lines = parseLyrics(source)
    emit('lyrics', {
      lines,
      synchronized: lines.some(line => line.timeMs > 0),
      status: lines.length ? '' : 'No lyrics available',
    })
  } catch (error) {
    // Lyrics availability varies by storefront and release. It is an optional
    // panel, so an unavailable endpoint must not poison playback with a global
    // error. The reason is still reported, because "no lyrics" and "the
    // request failed" look identical from the panel and are not the same
    // problem.
    emit('hook-warning', {
      detail: `lyrics lookup failed: ${(error && error.message) || error}`,
    })
    emit('lyrics', { lines: [], synchronized: false, status: 'No lyrics available' })
  }
}

// Adopts a session MusicKit has already rehydrated from the persisted
// partition. Returns false when there is nothing to adopt; never prompts.
function adoptExistingSession() {
  if (authorizationSettled) return true
  const existing = readTokens()
  if (!existing || !existing.musicUserToken) return false
  // Being able to read a token is not the same as MusicKit being authorized.
  // After a cold start the page can expose a token while its player is still
  // in preview mode; adopting that skipped authorize() for the rest of the
  // session, and every track then played a 90-second preview.
  if (pick(() => music.isAuthorized) !== true) return false
  authorizedUserToken = existing.musicUserToken
  authorizationSettled = true
  return true
}

async function ensureAuthorized() {
  if (!authorizationSettled) {
    // A restored MusicKit session is already usable. Calling authorize()
    // again here can reopen Apple's sign-in UI even though the token is
    // valid, especially while isAuthorized is still rehydrating.
    if (!adoptExistingSession()) {
      const token = await music.authorize()
      if (typeof token === 'string' && token.length > 0) authorizedUserToken = token
      authorizationSettled = true
    }
  }
  const tokens = pushTokens()
  if (!tokens || !tokens.musicUserToken) {
    authorizationSettled = false
    throw new Error('Apple Music authorization completed without a user token')
  }
  // music.apple.com initially configures the player for previews. MusicKit
  // normally clears this after authorization, but that transition is not
  // reliable when its page is hidden and only the Apple sign-in popup is
  // shown. Explicitly opt back into MusicKit's normal subscription decision.
  // `false` does not bypass a subscription check; it merely prevents the
  // instance from being forced to use the public 30-second assets.
  disablePreviewOnly()
  emit('authorization-settled', { authorized: true })
}

const commands = {
  async setQueue({ songs, startPosition = 0, startPlaying = true, startTimeMs = 0 }) {
    // A Music User Token can reappear before MusicKit has finished enabling
    // subscription playback. setQueue in that gap resolves to 30-second
    // previews. Settle MusicKit authorization before constructing the queue.
    await ensureAuthorized()
    disablePreviewOnly()
    await music.setQueue({ songs, startWith: startPosition, startPosition,
      startPlaying, startTime: startTimeMs / 1000 })
  },
  play: () => music.play(),
  pause: () => music.pause(),
  stop: () => music.stop(),
  playPause: () => music.isPlaying ? music.pause() : music.play(),
  next: () => music.skipToNextItem(),
  previous: () => music.skipToPreviousItem(),
  changeToIndex: ({ index }) => music.changeToMediaAtIndex(index),
  moveInQueue: ({ from, to }) => {
    if (typeof music.queue?.splice !== 'function') throw new Error('This MusicKit build cannot reorder the queue')
    const moved = music.queue.splice(from, 1)
    if (!moved || moved.length !== 1) throw new Error('Could not move that queue item')
    music.queue.splice(to, 0, moved[0])
    if (music.queue.position === from) music.queue.position = to
    emit('queue', queuePayload())
  },
  removeFromQueue: ({ index }) => {
    if (typeof music.queue?.remove === 'function') music.queue.remove(index)
    else if (typeof music.queue?.splice === 'function') music.queue.splice(index, 1)
    else throw new Error('This MusicKit build cannot remove queue items')
    emit('queue', queuePayload())
  },
  // Radio stations are queued through a play-parameters descriptor rather than
  // a song list. MusicKit builds differ on whether they accept the descriptor
  // or a bare station id, so try the documented form first and fall back.
  async playStation({ id }) {
    await ensureAuthorized()
    disablePreviewOnly()
    try {
      await music.setQueue({ playParams: { id, kind: 'radioStation' } })
    } catch (error) {
      await music.setQueue({ station: id })
    }
    await music.play()
    emit('queue', queuePayload())
  },
  playNext: ({ songs }) => enqueue('playNext', songs),
  playLater: ({ songs }) => enqueue('playLater', songs),
  getLyrics: ({ id }) => fetchLyrics(id),
  favorite: ({ id, type = 'songs' }) => {
    const resource = type.replace(/^library-/, '')
    return libraryWrite('favorite', id, () =>
      music.api.post(`/v1/me/favorites?ids[${resource}]=${encodeURIComponent(id)}`))
  },
  unfavorite: ({ id, type = 'songs' }) => {
    const resource = type.replace(/^library-/, '')
    return libraryWrite('unfavorite', id, () =>
      music.api.delete(`/v1/me/favorites?ids[${resource}]=${encodeURIComponent(id)}`))
  },
  addToLibrary: ({ id, type = 'songs' }) => {
    const resource = type.replace(/^library-/, '')
    return libraryWrite('add-library', id, () =>
      music.api.post(`/v1/me/library?ids[${resource}]=${encodeURIComponent(id)}`))
  },
  removeFromLibrary: ({ id, type = 'songs' }) => {
    const resource = type.replace(/^library-/, '')
    return libraryWrite('remove-library', id, () =>
      music.api.delete(`/v1/me/library/${resource}/${encodeURIComponent(id)}`))
  },
  seek: ({ positionMs }) => music.seekToTime(positionMs / 1000),
  setVolume: ({ volume }) => { music.volume = volume },
  setShuffle: ({ shuffle }) => {
    music.shuffleMode = shuffle ? 1 : 0
    emitModes()
    emit('queue', queuePayload())
  },
  setRepeat: ({ mode }) => {
    music.repeatMode = mode === 'one' ? 1 : mode === 'all' ? 2 : 0
    emitModes()
  },
  async authorize() {
    await ensureAuthorized()
  },
  // Silent counterpart to authorize(). It only ever adopts a session MusicKit
  // has already restored, so a launch with no saved session shows Kanzi's own
  // sign-in page instead of Apple's popup appearing unrequested.
  restoreAuthorization() {
    if (!adoptExistingSession()) {
      pushTokens()
      return
    }
    disablePreviewOnly()
    pushTokens()
    emit('authorization-settled', { authorized: true })
  },
  async unauthorize() {
    await music.unauthorize()
    authorizedUserToken = null
    authorizationSettled = false
    pushTokens()
  },
  refreshTokens: () => pushTokens(),
}

ipcRenderer.on('kanzi:command', async (_event, message) => {
  emit('cmd-recv', { cmd: message.cmd })
  const command = commands[message.cmd]
  if (!command) return emit('error', { code: 'unknown-command', detail: message.cmd })
  try {
    await command(message)
    emit('cmd-done', { cmd: message.cmd, state: pick(() => music.playbackState) ?? 0,
      queueLen: pick(() => music.queue.items.length) ?? 0 })
  } catch (error) {
    emit('error', { code: `command-${message.cmd}`, detail: String((error && error.message) || error) })
  }
})

function wire(trigger) {
  if (music) return true
  music = getInstance()
  if (!music) return false
  wireEvents()
  pushTokens()
  emit('hook-ready', { trigger, authorized: !!pick(() => music.isAuthorized),
    version: pick(() => window.MusicKit.version) || 'unknown' })
  emit('queue', queuePayload())
  emit('volume', { volume: pick(() => music.volume) ?? 1 })
  emitModes()
  return true
}

ipcRenderer.on('kanzi:wire', () => {
  if (!wire('main-probe')) emit('hook-failed', { detail: 'MusicKit vanished before the hook attached' })
})

function selfPoll() {
  const deadline = Date.now() + READY_TIMEOUT_MS
  const tick = () => {
    if (wire('self-poll') || Date.now() > deadline) return
    setTimeout(tick, READY_POLL_MS)
  }
  tick()
}

emit('hook-boot', { readyState: document.readyState, href: location.href })
if (document.readyState === 'loading') window.addEventListener('DOMContentLoaded', selfPoll, { once: true })
else selfPoll()
