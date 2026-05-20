add_executable(optix_tests test_pipeline.cpp)
target_link_libraries(optix_tests PRIVATE optix_core)
add_test(NAME optix_tests COMMAND optix_tests)
