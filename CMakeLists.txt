cmake_minimum_required(VERSION 3.28)
cmake_policy(VERSION 3.28)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(PROJECT_NAME "cxxmp")
set(CMAKE_CXX_COMPILER "clang++")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project("${PROJECT_NAME}" LANGUAGES CXX VERSION 0.0.1)

set(EXECUTABLE_OUTPUT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/bin")
set(LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/lib")
set(CXXMP_PROJECT_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(CXXMP_EXTERN_PACKAGE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extern")

# @NOTE: these `.cmake` order below should not be changed
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/Logging.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/Git.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/Versions.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/third-party/addSpdlog.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/Config.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/GetSysInfo.cmake")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/src")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/tests")
