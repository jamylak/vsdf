project(${PROJECT_NAME}_tests)
find_package(GTest REQUIRED)

# Include the Google Test library
include_directories(${GTEST_INCLUDE_DIRS})

# Add test executable
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

# Optional FFmpeg tests (only when FFmpeg is enabled)
if (VSDF_ENABLE_FFMPEG)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
    libavformat libavcodec libavutil libswscale)
  target_sources(${PROJECT_NAME} PRIVATE ../src/ffmpeg_utils.cpp test_ffmpeg_utils.cpp)
  target_link_libraries(${PROJECT_NAME} PkgConfig::FFMPEG)
endif()

# Discover and run tests
include(GoogleTest)
add_subdirectory(filewatcher)
gtest_discover_tests(${PROJECT_NAME})
