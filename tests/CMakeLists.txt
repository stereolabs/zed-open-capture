# Hardware tests, based on GoogleTest (`sudo apt install libgtest-dev`).
# They need at least one ZED camera connected and no display.
#
# Run them all with `ctest --output-on-failure` from the build folder, or run one
# executable directly to select cases, e.g. `tests/test_video --gtest_filter='*Controls*'`.

find_package(GTest REQUIRED)

function(zed_oc_add_test target)
    add_executable(${target} ${target}.cpp)
    target_link_libraries(${target} PRIVATE ${PROJECT_NAME} GTest::gtest GTest::gtest_main)
    add_test(NAME ${target} COMMAND ${target})
    # The tests share the cameras: never run two of them at the same time
    set_tests_properties(${target} PROPERTIES LABELS hardware RUN_SERIAL TRUE TIMEOUT 600)
endfunction()

if(BUILD_VIDEO)
    zed_oc_add_test(test_video)
    zed_oc_add_test(test_controls)
endif()

if(BUILD_SENSORS)
    zed_oc_add_test(test_sensors)
endif()

if(BUILD_VIDEO AND BUILD_SENSORS)
    zed_oc_add_test(test_sync)
endif()

# Long test, not started by `ctest` unless requested: `ctest -L stress` or `tests/test_stress`
if(BUILD_VIDEO)
    add_executable(test_stress test_stress.cpp)
    target_link_libraries(test_stress PRIVATE ${PROJECT_NAME} GTest::gtest GTest::gtest_main)
    add_test(NAME test_stress COMMAND test_stress CONFIGURATIONS Stress)
    set_tests_properties(test_stress PROPERTIES LABELS stress RUN_SERIAL TRUE TIMEOUT 7200)
endif()
