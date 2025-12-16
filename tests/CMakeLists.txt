project(${PROJECT_NAME}_tests)
find_package(GTest REQUIRED)

# Include the Google Test library
include_directories(${GTEST_INCLUDE_DIRS})

# Add test executable
add_executable(${PROJECT_NAME} test_shader_comp.cpp)
target_link_libraries(${PROJECT_NAME} ${GTEST_BOTH_LIBRARIES} pthread shader_utils_lib)

# Discover and run tests
include(GoogleTest)
add_subdirectory(filewatcher)
gtest_discover_tests(${PROJECT_NAME})
