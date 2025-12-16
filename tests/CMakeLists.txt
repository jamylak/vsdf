project(${PROJECT_NAME}_tests)
find_package(GTest REQUIRED)

# Include the Google Test library
include_directories(${GTEST_INCLUDE_DIRS})

# Add test executable
add_executable(${PROJECT_NAME} ../src/shader_utils.cpp test_shader_comp.cpp)

# Platform-specific threading library
if(UNIX)
  set(THREAD_LIB pthread)
else()
  set(THREAD_LIB "")
endif()

target_link_libraries(${PROJECT_NAME} ${GTEST_BOTH_LIBRARIES} ${THREAD_LIB} spdlog::spdlog glslang::glslang glslang::glslang-default-resource-limits glslang::SPIRV)

# Discover and run tests
include(GoogleTest)
add_subdirectory(filewatcher)
gtest_discover_tests(${PROJECT_NAME})
