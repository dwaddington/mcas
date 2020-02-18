/*
   eXokernel Development Kit (XDK)

   Samsung Research America Copyright (C) 2013
   IBM Corporation 2019

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.
   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <http://www.gnu.org/licenses/>.

   As a special exception, if you link the code in this file with
   files compiled with a GNU compiler to produce an executable,
   that does not cause the resulting executable to be covered by
   the GNU Lesser General Public License.  This exception does not
   however invalidate any other reasons why the executable file
   might be covered by the GNU Lesser General Public License.
   This exception applies to code released by its copyright holders
   in files containing the exception.
*/

/*
  Author(s):
  Copyright (C) 2016,2019, Daniel G. Waddington <daniel.waddington@ibm.com>
  Copyright (C) 2014, Daniel G. Waddington <daniel.waddington@acm.org>
*/

#ifndef __COMMON_LOGGING_H__
#define __COMMON_LOGGING_H__

#include <assert.h>
#include <stdio.h>
#include <stdarg.h>

#define NORMAL_CYAN "\033[36m"
#define NORMAL_MAGENTA "\033[35m"
#define NORMAL_BLUE "\033[34m"
#define NORMAL_YELLOW "\033[33m"
#define NORMAL_GREEN "\033[32m"
#define NORMAL_RED "\033[31m"

#define BRIGHT "\033[1m"
#define NORMAL_XDK "\033[0m"
#define RESET "\033[0m"

#define BRIGHT_CYAN "\033[1m\033[36m"
#define BRIGHT_MAGENTA "\033[1m\033[35m"
#define BRIGHT_BLUE "\033[1m\033[34m"
#define BRIGHT_YELLOW "\033[1m\033[33m"
#define BRIGHT_GREEN "\033[1m\033[32m"
#define BRIGHT_RED "\033[1m\033[31m"

#define WHITE_ON_RED "\033[41m"
#define WHITE_ON_GREEN "\033[42m"
#define WHITE_ON_YELLOW "\033[43m"
#define WHITE_ON_BLUE "\033[44m"
#define WHITE_ON_MAGENTA "\033[44m"

#define ESC_LOG NORMAL_GREEN
#define ESC_DBG NORMAL_YELLOW
#define ESC_INF NORMAL_CYAN
#define ESC_WRN NORMAL_RED
#define ESC_ERR BRIGHT_RED
#define ESC_END "\033[0m"

void pr_info(const char * format, ...) __attribute__((format(printf, 1, 2)));

inline void pr_info(const char * format, ...)
{
#ifdef CONFIG_DEBUG
  static constexpr size_t m_max_buffer = 512;
  va_list args;
  va_start(args, format);
  char buffer[m_max_buffer];
  vsnprintf(buffer, m_max_buffer, format, args);
  va_end(args);
  fprintf(stderr, "%s[LOG]:%s %s\n", ESC_LOG, buffer, ESC_END);
#else
  (void)format;
#endif
}

void pr_error(const char * format, ...) __attribute__((format(printf, 1, 2)));
inline void pr_error(const char * format, ...)
{
#ifdef CONFIG_DEBUG
  static constexpr size_t m_max_buffer = 512;
  va_list args;
  va_start(args, format);
  char buffer[m_max_buffer];
  vsnprintf(buffer, m_max_buffer, format, args);
  va_end(args);
  fprintf(stderr, "%s[LOG]:%s %s\n", ESC_ERR, buffer, ESC_END);
#else
  (void)format;
#endif
}

void PLOG(const char * format, ...) __attribute__((format(printf, 1, 2)));
inline void PLOG(const char * format, ...)
{
#ifdef CONFIG_DEBUG
  static constexpr size_t m_max_buffer = 512;
  va_list args;
  va_start(args, format);
  char buffer[m_max_buffer];
  vsnprintf(buffer, m_max_buffer, format, args);
  va_end(args);
  fprintf(stderr, "%s[LOG]:%s %s\n", ESC_LOG, buffer, ESC_END);
#else
  (void)format;
#endif
}

void PDBG(const char * format, ...) __attribute__((format(printf, 1, 2)));
inline void PDBG(const char * format, ...)
{
#ifdef CONFIG_DEBUG
  static constexpr size_t m_max_buffer = 512;
  va_list args;
  va_start(args, format);
  char buffer[m_max_buffer];
  vsnprintf(buffer, m_max_buffer, format, args);
  va_end(args);
  fprintf(stderr, "%s[DBG]:%s %s\n", ESC_DBG, buffer, ESC_END);
#else
  (void)format;
#endif
}

void PINF(const char * format, ...) __attribute__((format(printf, 1, 2)));
inline void PINF(const char * format, ...)
{
#ifdef CONFIG_DEBUG
  static constexpr size_t m_max_buffer = 512;
  va_list args;
  va_start(args, format);
  char buffer[m_max_buffer];
  vsnprintf(buffer, m_max_buffer, format, args);
  va_end(args);
  fprintf(stderr, "%s %s %s\n", ESC_INF, buffer, ESC_END);
#else
  (void)format;
#endif
}

void PWRN(const char * format, ...) __attribute__((format(printf, 1, 2)));
inline void PWRN(const char * format, ...)
{
#ifdef CONFIG_DEBUG
  static constexpr size_t m_max_buffer = 512;
  va_list args;
  va_start(args, format);
  char buffer[m_max_buffer];
  vsnprintf(buffer, m_max_buffer, format, args);
  va_end(args);
  fprintf(stderr, "%s[WRN]: %s %s\n", ESC_WRN, buffer, ESC_END);
#else
  (void)format;
#endif
}

void PERR(const char * format, ...) __attribute__((format(printf, 1, 2)));
inline void PERR(const char * format, ...)
{
#ifdef CONFIG_DEBUG
  static constexpr size_t m_max_buffer = 512;
  va_list args;
  va_start(args, format);
  char buffer[m_max_buffer];
  vsnprintf(buffer, m_max_buffer, format, args);
  va_end(args);
  fprintf(stderr, "%sError: %s %s\n", ESC_ERR, buffer, ESC_END);
#else
  (void)format;
#endif
}

void PEXCEP(const char * format, ...) __attribute__((format(printf, 1, 2)));
inline void PEXCEP(const char * format, ...)
{
#ifdef CONFIG_DEBUG
  static constexpr size_t m_max_buffer = 512;
  va_list args;
  va_start(args, format);
  char buffer[m_max_buffer];
  vsnprintf(buffer, m_max_buffer, format, args);
  va_end(args);
  fprintf(stderr, "%sException: %s %s\n", ESC_ERR, buffer, ESC_END);
#else
  (void)format;
#endif
}

void PNOTICE(const char * format, ...) __attribute__((format(printf, 1, 2)));
inline void PNOTICE(const char * format, ...)
{
#ifdef CONFIG_DEBUG
  static constexpr size_t m_max_buffer = 512;
  va_list args;
  va_start(args, format);
  char buffer[m_max_buffer];
  vsnprintf(buffer, m_max_buffer, format, args);
  va_end(args);
  fprintf(stderr, "%sNOTICE: %s %s\n", BRIGHT_RED, buffer, ESC_END);
#else
  (void)format;
#endif
}

void PMAJOR(const char * format, ...) __attribute__((format(printf, 1, 2)));
inline void PMAJOR(const char * format, ...)
{
#ifdef CONFIG_DEBUG
  static constexpr size_t m_max_buffer = 512;
  va_list args;
  va_start(args, format);
  char buffer[m_max_buffer];
  vsnprintf(buffer, m_max_buffer, format, args);
  va_end(args);
  fprintf(stderr, "%s[+] %s %s\n", NORMAL_BLUE, buffer, ESC_END);
#else
  (void)format;
#endif
}


#endif  // __COMMON_LOGGING_H__
