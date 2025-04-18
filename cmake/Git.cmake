find_package(Git QUIET)
if(GIT_FOUND)
    log_info("Git found")
endif()

# generate git versions
if(EXISTS "${CXXMP_PROJECT_ROOT_DIR}/.git")
    log_info("Git repository found")
else()
    log_fatal("Git repository not found, please checkout git repository")
endif()
