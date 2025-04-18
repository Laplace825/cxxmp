if (APPLE)
  execute_process(COMMAND xcrun --show-sdk-path
    OUTPUT_VARIABLE MACOS_SDK_PATH 
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )

  set(CMAKE_CXX_FLAGS 
    "${CMAKE_CXX_FLAGS} -stdlib=libc++ -isysroot ${MACOS_SDK_PATH}")
elseif(LINUX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif()

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_HEADER_TEMPLATE "${CXXMP_PROJECT_ROOT_DIR}/cmake/template/")

configure_file("${CMAKE_HEADER_TEMPLATE}/config.h.in" 
    "${CXXMP_PROJECT_ROOT_DIR}/include/config.h")
log_info("Generate config.h file >> include/config.h")
