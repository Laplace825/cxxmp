if(NOT TARGET spdlog)
    # Stand-alone build
    find_package(spdlog REQUIRED)
    if(NOT spdlog_FOUND)
        log_fatal("spdlog not found, please install it.")
    else()
        log_info("spdlog found")
    endif()
endif()
