#include <common/logging.h> /* NORMAL_CYAN, BRIGHT_RED, BRIGHT_CYAN, RESET */

#include <stdarg.h>
#include <stdarg.h>

#define PREFIX "MM-PLUGIN-RCALB:"

void pp_inner(const char *prefix, const char *format, va_list args)
{
#ifdef DEBUG
  char formatb[m_max_buffer];
  char buffer[m_max_buffer];
  vsnprintf(formatb, m_max_buffer, format, args);
# if 0
  snprintf(buffer, m_max_buffer, "%s%s%s\n", prefix, buffer, RESET);
  write(1, buffer, strlen(buffer));
# else
  printf(buffer, "%s%s%s\n", prefix, buffer, RESET);
# endif
#else
  (void)prefix;
  (void)format;
  (void)args;
#endif
}

void PPLOG(const char * format, ...)
{
  va_list args;
  va_start(args, format);
  pp_inner(NORMAL_CYAN PREFIX, format, args);
  va_end(args);
}

void PPERR(const char * format, ...)
{
  va_list args;
  va_start(args, format);
  pp_inner(BRIGHT_RED "error:", format, args);
  va_end(args);
}

void PPNOTICE(const char * format, ...)
{
  va_list args;
  va_start(args, format);
  pp_inner(BRIGHT_CYAN PREFIX, format, args);
  va_end(args);
}
