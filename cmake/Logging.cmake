string(ASCII 27 Esc)

set(LOG_RED "${Esc}[0;31m")
set(LOG_GREEN "${Esc}[0;32m")
set(LOG_YELLOW "${Esc}[0;33m")
set(LOG_BLUE "${Esc}[0;34m")
set(LOG_PURPLE "${Esc}[0;35m")
set(LOG_CYAN "${Esc}[0;36m")
set(LOG_WHITE "${Esc}[0;37m")
set(LOG_RESET "${Esc}[m")

set(LOG_CAT "${LOG_BLUE}ᓚᘏᗢ${LOG_RESET}")

function(log_info msg)
    message(STATUS "[${LOG_GREEN}INFO${LOG_RESET}] >>> ${LOG_CAT} ${msg}")
endfunction(log_info msg)

function(log_warning msg)
    message("[${LOG_YELLOW}WARNING${LOG_RESET}] >>> ${LOG_CAT} ${LOG_YELLOW}${msg}${LOG_RESET}")
endfunction(log_warning msg)

function(log_error msg)
    message(SEND_ERROR "[${LOG_RED}ERROR${LOG_RESET}] >>> ${LOG_CAT} ${msg}")
endfunction(log_error msg)

function(log_fatal msg)
    message(FATAL_ERROR "[${LOG_RED}FATAL${LOG_RESET}] >>> ${LOG_CAT} ${msg}")
endfunction(log_fatal msg)
