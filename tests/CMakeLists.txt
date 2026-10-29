add_executable(noble_tests
    test_main.cpp
)
target_link_libraries(noble_tests PRIVATE noblecore Qt6::Test)
add_test(NAME noble_tests COMMAND noble_tests)

qt_add_resources(noble_tests "noble_test_assets"
    PREFIX "/assets"
    BASE "${CMAKE_SOURCE_DIR}/assets"
    FILES
        ${CMAKE_SOURCE_DIR}/assets/noble-builds-manifest.json
        ${CMAKE_SOURCE_DIR}/assets/noble-builds/noble-sample.zip
        ${CMAKE_SOURCE_DIR}/assets/noble-builds/nvidia-mid.zip
        ${CMAKE_SOURCE_DIR}/assets/noble-builds/nvidia-low.zip
        ${CMAKE_SOURCE_DIR}/assets/noble-builds/amd-mid.zip
        ${CMAKE_SOURCE_DIR}/assets/noble-builds/amd-low.zip
        ${CMAKE_SOURCE_DIR}/assets/noble-builds/intel-igpu.zip
        ${CMAKE_SOURCE_DIR}/assets/noble-builds/max-fps.zip
        ${CMAKE_SOURCE_DIR}/assets/noble-builds/balanced-visuals.zip
        ${CMAKE_SOURCE_DIR}/assets/noble-builds/low-end-laptop.zip
        ${CMAKE_SOURCE_DIR}/assets/noble-builds/pvp.zip
        ${CMAKE_SOURCE_DIR}/assets/noble-builds/streaming.zip
)
