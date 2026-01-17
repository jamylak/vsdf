project(${PROJECT_NAME}_tests)
find_package(GTest REQUIRED)

set(TEST_SOURCES
  ../src/shader_utils.cpp
  test_shader_comp.cpp
  test_frame.cpp
  test_online_ppm_dump.cpp
)

# Common libraries for all platforms
set(COMMON_LIBS GTest::gtest_main spdlog::spdlog_header_only glslang::glslang glslang::glslang-default-resource-limits glslang::SPIRV glfw volk)

if (VSDF_ENABLE_FFMPEG)
  list(APPEND TEST_SOURCES
    test_offline_ppm_dump.cpp
    test_offline_utils.cpp
  )
endif()

# Add test executable
add_executable(${PROJECT_NAME} ${TEST_SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include ${Vulkan_INCLUDE_DIRS})

# Platform-specific threading library
if(UNIX)
  find_package(Threads REQUIRED)
  target_link_libraries(${PROJECT_NAME} PRIVATE ${COMMON_LIBS} Threads::Threads)
else()
  # Windows doesn't need explicit pthread library
  target_link_libraries(${PROJECT_NAME} PRIVATE ${COMMON_LIBS})
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE
  VSDF_BINARY_PATH="${CMAKE_BINARY_DIR}/vsdf"
  VSDF_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
  SHADER_DIR="${CMAKE_SOURCE_DIR}/shaders/"
)

if (VSDF_ENABLE_FFMPEG)
  if (WIN32)
    find_package(FFMPEG CONFIG REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE
      FFMPEG::avformat
      FFMPEG::avcodec
      FFMPEG::avutil
      FFMPEG::swscale)
  else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
      libavformat libavcodec libavutil libswscale)
    target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::FFMPEG)
  endif()
  target_sources(${PROJECT_NAME} PRIVATE ../src/ffmpeg_utils.cpp
    ../src/ffmpeg_encoder.cpp
    test_ffmpeg.cpp
    test_ffmpeg_encode.cpp
    test_offline_ffmpeg_encode.cpp)
  target_compile_definitions(${PROJECT_NAME} PRIVATE VSDF_ENABLE_FFMPEG=1)
endif()

# Discover and run tests
include(GoogleTest)
add_subdirectory(filewatcher)
gtest_discover_tests(${PROJECT_NAME})
