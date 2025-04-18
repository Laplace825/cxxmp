# get all files in ./pl0
file(GLOB_RECURSE SOURCES_FILES "${CMAKE_CURRENT_SOURCE_DIR}/cxxmp/*.cc")

set(CXXMP_LIB_NAME "cxxmp" CACHE STRING "Project library name")

add_library("${CXXMP_LIB_NAME}" STATIC ${SOURCES_FILES})
add_library(cxxmp::cxxmp ALIAS "${CXXMP_LIB_NAME}")

# set the lib path
set_target_properties(${CXXMP_LIB_NAME} PROPERTIES
    OUTPUT_DIRECTORY "${CXXMP_PROJECT_ROOT_DIR}/lib"
    ARCHIVE_OUTPUT_DIRECTORY "${CXXMP_PROJECT_ROOT_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CXXMP_PROJECT_ROOT_DIR}/lib"
)

target_include_directories(${CXXMP_LIB_NAME} PUBLIC
    $<BUILD_INTERFACE:${CXXMP_PROJECT_ROOT_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(${CXXMP_LIB_NAME} PUBLIC
  spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>)
