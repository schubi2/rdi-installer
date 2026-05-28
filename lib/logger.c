// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include <errno.h>
#include <stdarg.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <systemd/sd-journal.h>
#include "logger.h"

static FILE *log_file = NULL;
static int current_log_level = LOG_TRACE;

const char* log_level_to_str(int level) {
    switch (level) {
        case LOG_EMERG:   return "EMERGENCY";
        case LOG_ALERT:   return "ALERT";
        case LOG_CRIT:    return "CRITICAL";
        case LOG_ERR:     return "ERROR";
        case LOG_WARNING: return "WARNING";
        case LOG_NOTICE:  return "NOTICE";
        case LOG_INFO:    return "INFO";
        case LOG_DEBUG:   return "DEBUG";
        default:          return "UNKNOWN";
    }
}

int
log_init(const char *filename)
{
  if (log_file)
    return 0;

  log_file = fopen(filename, "a");
  if (!log_file)
    return -errno;
  return 0;
}

void
set_max_log_level(int level)
{
  current_log_level = level;
}

void
log_close(void)
{
  if (log_file)
    {
      fclose(log_file);
      log_file = NULL;
    }
}

void
log_write_service(int level, const char *fmt, ...)
{
  static int is_tty = -1;

  if (level == LOG_TRACE || /* Do not log function parameters */
      level > current_log_level)
    return;

  if (is_tty == -1)
    is_tty = isatty(STDOUT_FILENO);

  va_list ap;

  va_start(ap, fmt);

  if (is_tty)
    {
      if (level <= LOG_ERR)
        {
          vfprintf(stderr, fmt, ap);
          fputc('\n', stderr);
        }
      else
        {
          vprintf(fmt, ap);
          putchar('\n');
        }
    }
  else
    sd_journal_printv(level, fmt, ap);

  va_end(ap);
}

void
log_write(int level, const char *file, int line, const char *func,
	  const char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);

  if (!log_file)
    {
      log_write_service( level, fmt, args);
    } else {
      time_t now;
      time(&now);
      struct tm *tm_info = localtime(&now);
      char time_buffer[26];
      strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", tm_info);

      fprintf(log_file, "[%s] [%-5s] %s:%d %s() - ",
	      time_buffer, log_level_to_str(level), file, line, func);


      vfprintf(log_file, fmt, args);
      fprintf(log_file, "\n");
      fflush(log_file);
    }
  va_end(args);
}
