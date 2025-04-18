add_executable(${PROJECT_NAME}-Test ${CMAKE_CURRENT_SOURCE_DIR}/test.cc)
target_include_directories(${PROJECT_NAME}-Test 
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(${PROJECT_NAME}-Test PUBLIC
  spdlog::spdlog $<$<BOOL:${MINGW}>:ws2_32>
  "${CXXMP_LIB_NAME}"
)
