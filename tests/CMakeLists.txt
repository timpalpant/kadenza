qt_add_executable(kadenza-model-tests
    mediamodeltest.cpp
    ../src/media/mediaitem.cpp ../src/media/mediaitem.h
    ../src/media/mediamodel.cpp ../src/media/mediamodel.h)
target_include_directories(kadenza-model-tests PRIVATE ../src/media)
target_link_libraries(kadenza-model-tests PRIVATE Qt6::Core Qt6::Test Qt6::Qml)
add_test(NAME mediamodel COMMAND kadenza-model-tests)


qt_add_executable(kadenza-cache-tests
    librarycachetest.cpp
    ../src/data/database.cpp ../src/data/database.h
    ../src/data/librarycache.cpp ../src/data/librarycache.h
    ../src/media/mediaitem.cpp ../src/media/mediaitem.h)
target_include_directories(kadenza-cache-tests PRIVATE ../src/data ../src/media)
target_link_libraries(kadenza-cache-tests PRIVATE Qt6::Core Qt6::Test Qt6::Sql)
add_test(NAME librarycache COMMAND kadenza-cache-tests)
