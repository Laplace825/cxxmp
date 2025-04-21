# if(UNIX AND NOT APPLE)
#     log_info("Linux system detected, using /proc/cpuinfo to get CPU count")
#     execute_process(COMMAND grep -c ^processor /proc/cpuinfo
#         OUTPUT_VARIABLE CXXMP_PROC_COUNT
#         ERROR_QUIET
#         OUTPUT_STRIP_TRAILING_WHITESPACE
#         RESULT_VARIABLE CXXMP_PROC_COUNT
# )
# elseif(APPLE)
#     log_info("Apple system detected, using sysctl to get CPU count")
#     execute_process(COMMAND sysctl -n hw.ncpu
#         OUTPUT_VARIABLE CXXMP_PROC_COUNT
#         OUTPUT_STRIP_TRAILING_WHITESPACE
#         RESULT_VARIABLE CXXMP_PROC_COUNT
# )
# endif()

include(ProcessorCount)
ProcessorCount(CXXMP_PROC_COUNT)
if(NOT CXXMP_PROC_COUNT EQUAL 0)
    log_info("Processor count: ${CXXMP_PROC_COUNT}")
    # cache the processor count
    set(CXXMP_PROC_COUNT ${CXXMP_PROC_COUNT} CACHE STRING "Processor count")
endif()
