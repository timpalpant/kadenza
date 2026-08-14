qt_add_executable(kadenza-model-tests
    mediamodeltest.cpp
    ../src/media/mediaitem.cpp ../src/media/mediaitem.h
    ../src/media/mediamodel.cpp ../src/media/mediamodel.h)
target_include_directories(kadenza-model-tests PRIVATE ../src/media)
target_link_libraries(kadenza-model-tests PRIVATE Qt6::Core Qt6::Test Qt6::Qml)
add_test(NAME mediamodel COMMAND kadenza-model-tests)


qt_add_executable(kadenza-filter-tests
    mediafilterproxymodeltest.cpp
    ../src/media/mediaitem.cpp ../src/media/mediaitem.h
    ../src/media/mediamodel.cpp ../src/media/mediamodel.h
    ../src/media/mediafilterproxymodel.cpp ../src/media/mediafilterproxymodel.h)
target_include_directories(kadenza-filter-tests PRIVATE ../src/media)
target_link_libraries(kadenza-filter-tests PRIVATE Qt6::Core Qt6::Test Qt6::Qml)
add_test(NAME mediafilterproxy COMMAND kadenza-filter-tests)


qt_add_executable(kadenza-cache-tests
    librarycachetest.cpp
    ../src/data/database.cpp ../src/data/database.h
    ../src/data/librarycache.cpp ../src/data/librarycache.h
    ../src/media/mediaitem.cpp ../src/media/mediaitem.h)
target_include_directories(kadenza-cache-tests PRIVATE ../src/data ../src/media)
target_link_libraries(kadenza-cache-tests PRIVATE Qt6::Core Qt6::Test Qt6::Sql)
add_test(NAME librarycache COMMAND kadenza-cache-tests)


qt_add_executable(kadenza-apipaths-tests
    apipathstest.cpp
    ../src/api/apipaths.cpp ../src/api/apipaths.h)
target_include_directories(kadenza-apipaths-tests PRIVATE ../src/api)
target_link_libraries(kadenza-apipaths-tests PRIVATE Qt6::Core Qt6::Test)
add_test(NAME apipaths COMMAND kadenza-apipaths-tests)


qt_add_executable(kadenza-queueids-tests
    queueidstest.cpp
    ../src/player/queueids.cpp ../src/player/queueids.h)
target_include_directories(kadenza-queueids-tests PRIVATE ../src/player)
target_link_libraries(kadenza-queueids-tests PRIVATE Qt6::Core Qt6::Test)
add_test(NAME queueids COMMAND kadenza-queueids-tests)


qt_add_executable(kadenza-sync-tests
    librarysyncworkertest.cpp
    ../src/data/database.cpp ../src/data/database.h
    ../src/data/librarycache.cpp ../src/data/librarycache.h
    ../src/data/librarysyncworker.cpp ../src/data/librarysyncworker.h
    ../src/media/mediaitem.cpp ../src/media/mediaitem.h)
target_include_directories(kadenza-sync-tests PRIVATE ../src/data ../src/media)
target_link_libraries(kadenza-sync-tests PRIVATE Qt6::Core Qt6::Test Qt6::Sql)
add_test(NAME librarysyncworker COMMAND kadenza-sync-tests)


qt_add_executable(kadenza-artwork-tests
    artworkcachetest.cpp
    ../src/api/artworkcache.cpp ../src/api/artworkcache.h)
target_include_directories(kadenza-artwork-tests PRIVATE ../src/api)
target_link_libraries(kadenza-artwork-tests PRIVATE Qt6::Core Qt6::Gui Qt6::Test Qt6::Network Qt6::Qml Qt6::Quick)
add_test(NAME artworkcache COMMAND kadenza-artwork-tests)
# Needs a QGuiApplication for image decoding, but no display.
set_tests_properties(artworkcache PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
