cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 20)
project(vsdf)

find_package(glfw3 REQUIRED)
find_package(Vulkan REQUIRED)
find_package(glslang REQUIRED)
find_package(spdlog REQUIRED)
find_package(glm REQUIRED)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_compile_options(-fsanitize=address)
  add_link_options(-fsanitize=address)
endif()

add_executable(${PROJECT_NAME} src/main.cpp src/shader_utils.cpp src/sdf_renderer.cpp)

if(APPLE)
  message("Building filewatcher for macOS")
  target_sources(${PROJECT_NAME} PRIVATE src/filewatcher/mac_filewatcher.cpp)
  find_library(CORE_SERVICES_LIBRARY CoreServices)
  target_link_libraries(${PROJECT_NAME} PRIVATE ${CORE_SERVICES_LIBRARY})
elseif(UNIX AND NOT APPLE)
  # Print building linux
  message("Building filewatcher for Linux")
  find_package(Threads REQUIRED)
  target_sources(${PROJECT_NAME} PRIVATE src/filewatcher/linux_filewatcher.cpp)
  target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE glfw Vulkan::Vulkan spdlog::spdlog glslang::glslang glslang::glslang-default-resource-limits glslang::SPIRV)
include_directories(${PROJECT_NAME} PRIVATE include ${GLM_INCLUDE_DIRS})

# Option to enable or disable the building of tests
option(BUILD_TESTS "Build the tests" OFF)

if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
