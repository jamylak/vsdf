project(${PROJECT_NAME}_tests)
find_package(GTest REQUIRED)

# Include the Google Test library
include_directories(${GTEST_INCLUDE_DIRS})

# Add test executable for shader compilation
add_executable(${PROJECT_NAME} ../src/shader_utils.cpp test_shader_comp.cpp)

# Common libraries for all platforms
set(COMMON_LIBS ${GTEST_BOTH_LIBRARIES} spdlog::spdlog glslang::glslang glslang::glslang-default-resource-limits glslang::SPIRV)

# Platform-specific threading library
if(UNIX)
  find_package(Threads REQUIRED)
  target_link_libraries(${PROJECT_NAME} ${COMMON_LIBS} Threads::Threads)
else()
  # Windows doesn't need explicit pthread library
  target_link_libraries(${PROJECT_NAME} ${COMMON_LIBS})
endif()

# Add test executable for FFmpeg encoder
add_executable(ffmpeg_encoder_tests ../src/ffmpeg_encoder.cpp test_ffmpeg_encoder.cpp)
target_include_directories(ffmpeg_encoder_tests PRIVATE ${FFMPEG_INCLUDE_DIRS})

if(UNIX)
  target_link_libraries(ffmpeg_encoder_tests ${GTEST_BOTH_LIBRARIES} spdlog::spdlog ${FFMPEG_LIBRARIES} Threads::Threads)
else()
  target_link_libraries(ffmpeg_encoder_tests ${GTEST_BOTH_LIBRARIES} spdlog::spdlog ${FFMPEG_LIBRARIES})
endif()

# Discover and run tests
include(GoogleTest)
add_subdirectory(filewatcher)
gtest_discover_tests(${PROJECT_NAME})
gtest_discover_tests(ffmpeg_encoder_tests)
