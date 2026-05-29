#ifndef LOGGER_H
#define LOGGER_H

#include <sys/syslog.h>
#include <stdio.h>

#define LOG_TRACE   8

// Initialize and close the logger
int log_init(const char *filename); /* filename = NULL; Logging to console or via systemd */
void set_max_log_level(int level);
void log_close(void);

// The core logging functions (do not call directly, use macros)
void log_write(int level, const char *file, int line, const char *func, const char *fmt, ...);
const char* log_level_to_str(int level);

// Convenience Macros
// -----------------------------------------------------------------------------
// Call this at the start of a function to log its execution and arguments
#define LOG_FUN(...) \
    log_write(LOG_TRACE, __FILE__, __LINE__, __func__, "CALLED with args: " __VA_ARGS__)

// General purpose logging macros for use anywhere
#define LOG_INF(...)  log_write(LOG_INFO,    __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_WAR(...)  log_write(LOG_WARNING, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_ER(...)   log_write(LOG_ERR,   __FILE__, __LINE__, __func__, __VA_ARGS__)

#endif // LOGGER_H
