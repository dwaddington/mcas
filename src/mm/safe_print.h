#ifndef __SAFE_PRINT_H__
#define __SAFE_PRINT_H__

#include <common/logging.h> /* NORMAL_CYAN, RESET */

#include <stdarg.h> /* va_{list,start,end} */
#include <string.h> /* sprintf, vsnprintf */
#include <unistd.h> /* write */

#if defined __cplusplus
extern "C" {
#endif

void SAFE_PRINT(const char * format, ...) __attribute__((format(printf, 1, 2)));

inline void SAFE_PRINT(const char * format, ...)
{
#if defined DEBUG || LOGGING_ENABLE
  enum { m_max_buffer = 512 };
  va_list args;
  va_start(args, format);
  char buffer[m_max_buffer];
  char formatb[m_max_buffer];
  sprintf(formatb, "%s%s%s\n", NORMAL_CYAN, format, RESET);
  vsnprintf(buffer, m_max_buffer, formatb, args);
  va_end(args);
  write(1, buffer, strlen(buffer));
#else
  (void)format;
#endif
}

#if defined __cplusplus
}
#endif

#endif
