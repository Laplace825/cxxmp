if(GIT_FOUND)
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
    WORKING_DIRECTORY ${CXXMP_PROJECT_ROOT_DIR}
        OUTPUT_VARIABLE PACKAGE_GIT_HEAD
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE GIT_RESULT
    )
    # get the current branch name
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
    WORKING_DIRECTORY ${CXXMP_PROJECT_ROOT_DIR}
        OUTPUT_VARIABLE PACKAGE_GIT_BRANCH
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE GIT_RESULT
    )
    log_info("Build Type: ${CMAKE_BUILD_TYPE}")
    log_info("Git Head: ${PACKAGE_GIT_HEAD}")
    log_info("Git Branch: ${PACKAGE_GIT_BRANCH}")
endif()
