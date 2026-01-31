option(ENABLE_SANITIZERS "Build with sanitizers" OFF)
option(ENABLE_FUZZING "Build fuzz targets" OFF)
option(DISABLE_FFMPEG "Disable FFmpeg/offline renderer" OFF)

if (DISABLE_FFMPEG)
  set(VSDF_ENABLE_FFMPEG OFF)
else()
  set(VSDF_ENABLE_FFMPEG ON)
endif()

if (ENABLE_SANITIZERS)
  add_compile_options(-fsanitize=address,undefined -fno-sanitize=vptr -fno-omit-frame-pointer -g)
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

# Prefer static libraries for Release builds (except FFmpeg which has too many deps)
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Release build - preferring static libraries where practical")
    # Note: We keep FFmpeg and system libraries (libstdc++) dynamic
    if(WIN32)
        # Windows static and import libs use .lib; keep defaults for other suffixes.
        set(CMAKE_FIND_LIBRARY_SUFFIXES ".lib" ".a")
    else()
        set(CMAKE_FIND_LIBRARY_SUFFIXES ".a" ".dylib" ".so")
    endif()
else()
    message(STATUS "Debug build - using dynamic libraries")
    if(NOT WIN32)
        set(CMAKE_FIND_LIBRARY_SUFFIXES ".dylib" ".so" ".a")
    endif()
endif()

# For Release builds, try to find static versions of key libraries
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    # Try to find static glfw
    find_library(GLFW_STATIC_LIB NAMES libglfw3.a glfw3 PATHS /opt/homebrew/opt/glfw/lib /usr/local/lib NO_DEFAULT_PATH)
    if(GLFW_STATIC_LIB)
        message(STATUS "Found static glfw: ${GLFW_STATIC_LIB}")
        set(glfw3_FOUND TRUE)
    else()
        find_package(glfw3 REQUIRED)
    endif()
    
    find_package(spdlog REQUIRED)
else()
    find_package(glfw3 REQUIRED)
    find_package(spdlog REQUIRED)
endif()

# Vulkan: Only need headers for volk, not the loader library or tools
find_package(Vulkan REQUIRED)
find_package(glslang REQUIRED)
find_package(glm REQUIRED)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if (CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT MSVC)
  add_compile_options(-fsanitize=address)
  add_link_options(-fsanitize=address)
endif()

# Add volk for Vulkan meta-loader
add_subdirectory(external/volk)

add_executable(${PROJECT_NAME} src/main.cpp src/shader_utils.cpp src/sdf_renderer.cpp src/online_sdf_renderer.cpp src/image_dump.cpp)

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
  
  # RPATH settings for finding bundled libraries
  # @executable_path/libs - for bundled dylibs in release packages
  # /usr/local/lib, /opt/homebrew/lib - for development builds with Homebrew
  set_target_properties(${PROJECT_NAME} PROPERTIES
      BUILD_RPATH "@executable_path/libs;/usr/local/lib;/opt/homebrew/lib"
      INSTALL_RPATH "@executable_path/libs;/usr/local/lib;/opt/homebrew/lib"
  )
  
  # When static linking GLFW on macOS, we need these frameworks
  if(CMAKE_BUILD_TYPE STREQUAL "Release" AND GLFW_STATIC_LIB)
    find_library(COCOA_LIBRARY Cocoa REQUIRED)
    find_library(IOKIT_LIBRARY IOKit REQUIRED)
    find_library(COREVIDEO_LIBRARY CoreVideo REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${COCOA_LIBRARY} ${IOKIT_LIBRARY} ${COREVIDEO_LIBRARY})
  endif()
elseif(UNIX AND NOT APPLE)
  # Print building linux
  message("Building filewatcher for Linux")
  find_package(Threads REQUIRED)
  target_sources(${PROJECT_NAME} PRIVATE src/filewatcher/linux_filewatcher.cpp)
  target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)
  
  # RPATH settings for finding bundled libraries on Linux
  set_target_properties(${PROJECT_NAME} PROPERTIES
      BUILD_RPATH "$ORIGIN/libs:$ORIGIN"
      INSTALL_RPATH "$ORIGIN/libs:$ORIGIN"
  )
elseif(WIN32)
  message("Building filewatcher for Windows")
  target_sources(${PROJECT_NAME} PRIVATE src/filewatcher/windows_filewatcher.cpp)
endif()

# Link libraries - use static versions when found for Release builds
if(CMAKE_BUILD_TYPE STREQUAL "Release" AND GLFW_STATIC_LIB)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${GLFW_STATIC_LIB})
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE glfw)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE volk)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(SPDLOG_TARGET spdlog::spdlog_header_only)
else()
    set(SPDLOG_TARGET spdlog::spdlog)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE ${SPDLOG_TARGET})

target_link_libraries(${PROJECT_NAME} PRIVATE glslang::glslang glslang::glslang-default-resource-limits glslang::SPIRV)

# Include Vulkan headers for types (volk needs them)
target_include_directories(${PROJECT_NAME} PRIVATE ${Vulkan_INCLUDE_DIRS})
include_directories(${PROJECT_NAME} PRIVATE include ${GLM_INCLUDE_DIRS})

if (VSDF_ENABLE_FFMPEG)
  target_sources(${PROJECT_NAME} PRIVATE src/offline_sdf_renderer.cpp src/ffmpeg_utils.cpp src/ffmpeg_encoder.cpp)
  if (WIN32)
    find_package(FFMPEG CONFIG REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE
      FFMPEG::avformat
      FFMPEG::avcodec
      FFMPEG::avutil
      FFMPEG::swscale)
  else()
    # Temporarily restore dynamic library preference for FFmpeg (too many static deps)
    set(_SAVED_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES})
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".dylib" ".so" ".a")
    
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
      libavformat libavcodec libavutil libswscale)
    target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::FFMPEG)
    
    # Restore library suffix preference
    set(CMAKE_FIND_LIBRARY_SUFFIXES ${_SAVED_SUFFIXES})
  endif()
  target_compile_definitions(${PROJECT_NAME} PRIVATE VSDF_ENABLE_FFMPEG=1)
endif()

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
