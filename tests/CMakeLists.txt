qt_add_executable(kanzi-model-tests
    mediamodeltest.cpp
    ../src/media/mediaitem.cpp ../src/media/mediaitem.h
    ../src/media/mediamodel.cpp ../src/media/mediamodel.h)
target_include_directories(kanzi-model-tests PRIVATE ../src/media)
target_link_libraries(kanzi-model-tests PRIVATE Qt6::Core Qt6::Test Qt6::Qml)
add_test(NAME mediamodel COMMAND kanzi-model-tests)


qt_add_executable(kanzi-cache-tests
    librarycachetest.cpp
    ../src/data/database.cpp ../src/data/database.h
    ../src/data/librarycache.cpp ../src/data/librarycache.h
    ../src/media/mediaitem.cpp ../src/media/mediaitem.h)
target_include_directories(kanzi-cache-tests PRIVATE ../src/data ../src/media)
target_link_libraries(kanzi-cache-tests PRIVATE Qt6::Core Qt6::Test Qt6::Sql)
add_test(NAME librarycache COMMAND kanzi-cache-tests)
