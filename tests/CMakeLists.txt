add_executable(test_seamline test_stitch.cpp)
target_link_libraries(test_seamline PRIVATE seamline Catch2::Catch2WithMain)
target_include_directories(test_seamline PRIVATE ${CMAKE_SOURCE_DIR}/src)

include(CTest)
include(Catch)
catch_discover_tests(test_seamline)
