option(ENABLE_SANITIZERS "Build with sanitizers" OFF)
option(ENABLE_FUZZING "Build fuzz targets" OFF)

if (ENABLE_SANITIZERS)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
  add_link_options(-fsanitize=address,undefined)
endif()

if (ENABLE_FUZZING)
  add_executable(fuzz_target fuzz.cpp)
  target_compile_options(fuzz_target PRIVATE -fsanitize=fuzzer,address,undefined -O1 -g)
  target_link_options(fuzz_target PRIVATE -fsanitize=fuzzer,address,undefined)
endif()

cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 20)
project(vsdf)

find_package(glfw3 REQUIRED)
find_package(Vulkan REQUIRED)
find_package(glslang REQUIRED)
find_package(spdlog REQUIRED)
find_package(glm REQUIRED)

# Find FFmpeg libraries for video encoding
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED
    libavcodec
    libavformat
    libavutil
    libswscale
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if (CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT MSVC)
  add_compile_options(-fsanitize=address)
  add_link_options(-fsanitize=address)
endif()

add_executable(${PROJECT_NAME} src/main.cpp src/shader_utils.cpp src/sdf_renderer.cpp src/ffmpeg_encoder.cpp)

# Recommended warnings and safeguards
if(MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE
    /W4
  )
else()
  target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall -Wextra -Wpedantic -Werror
    -Wshadow -Wnon-virtual-dtor
    # -Wold-style-cast # (disabled as it complains about vulkan)
    -Wcast-align
    # -Wunused
    -Woverloaded-virtual -Wconversion -Wsign-conversion -Wmisleading-indentation
    -Wnull-dereference -Wdouble-promotion -Wformat=2
    -Wno-missing-field-initializers
    -Wno-shadow
  )
endif()
if(APPLE)
  message("Building filewatcher for macOS")
  target_sources(${PROJECT_NAME} PRIVATE src/filewatcher/mac_filewatcher.cpp)
  find_library(CORE_SERVICES_LIBRARY CoreServices)
  target_link_libraries(${PROJECT_NAME} PRIVATE ${CORE_SERVICES_LIBRARY})
  # Lunar https://vulkan.lunarg.com/doc/sdk/1.4.328.1/mac/getting_started.html
  set_target_properties(${PROJECT_NAME} PROPERTIES
      BUILD_RPATH "/usr/local/lib"
      INSTALL_RPATH "/usr/local/lib"
  )
elseif(UNIX AND NOT APPLE)
  # Print building linux
  message("Building filewatcher for Linux")
  find_package(Threads REQUIRED)
  target_sources(${PROJECT_NAME} PRIVATE src/filewatcher/linux_filewatcher.cpp)
  target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)
elseif(WIN32)
  message("Building filewatcher for Windows")
  target_sources(${PROJECT_NAME} PRIVATE src/filewatcher/windows_filewatcher.cpp)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE glfw Vulkan::Vulkan spdlog::spdlog glslang::glslang glslang::glslang-default-resource-limits glslang::SPIRV ${FFMPEG_LIBRARIES})
include_directories(${PROJECT_NAME} PRIVATE include ${GLM_INCLUDE_DIRS} ${FFMPEG_INCLUDE_DIRS})

# Install the executable
install(TARGETS ${PROJECT_NAME}
  RUNTIME DESTINATION bin
)

# Option to enable or disable the building of tests
option(BUILD_TESTS "Build the tests" OFF)

if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
