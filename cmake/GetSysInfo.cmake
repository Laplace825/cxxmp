include(ProcessorCount)
ProcessorCount(CXXMP_PROC_COUNT)
if(NOT CXXMP_PROC_COUNT EQUAL 0)
    log_info("Processor count: ${CXXMP_PROC_COUNT}")
    # cache the processor count
    set(CXXMP_PROC_COUNT ${CXXMP_PROC_COUNT} CACHE STRING "Processor count")
endif()
