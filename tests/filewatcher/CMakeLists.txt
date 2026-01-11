cmake_minimum_required(VERSION 3.10)

project(filewatcher_tests)

find_package(GTest REQUIRED)
find_package(spdlog REQUIRED)

set(SOURCE_FILES test_filewatcher.cpp)
if(APPLE)
  list(APPEND SOURCE_FILES ../../src/filewatcher/mac_filewatcher.cpp)
  find_library(CORE_SERVICES_LIBRARY CoreServices)
  set(EXTRA_LIBS ${CORE_SERVICES_LIBRARY})
elseif(UNIX AND NOT APPLE)
  list(APPEND SOURCE_FILES ../../src/filewatcher/linux_filewatcher.cpp)
  set(EXTRA_LIBS pthread)  # Typically needed for Linux builds
elseif(WIN32)
  list(APPEND SOURCE_FILES ../../src/filewatcher/windows_filewatcher.cpp)
  set(EXTRA_LIBS "")  # No extra libs needed for Windows
endif()

add_executable(${PROJECT_NAME} ${SOURCE_FILES})
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(${PROJECT_NAME} GTest::gtest_main ${EXTRA_LIBS} spdlog::spdlog)

if (CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE -fsanitize=address)
  target_link_options(${PROJECT_NAME} PRIVATE -fsanitize=address)
endif()

include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME})
