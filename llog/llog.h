/*
 * C Header file: llog.h
 */
#ifndef LLOG_GUARD_H
#define LLOG_GUARD_H 1

/**
 * @file
 * @brief An event log utility.
 *
 * This module is meant to be used in conjunction with another existing project.
 */

#if defined(__STDC_VERSION__)
#  if (__STDC_VERSION__ >= 201112L)
#    if !defined(__STDC_NO_THREADS__)
#      define USE_C11THREADS_ 1
#    endif
#  endif
#  if defined(__unix__) && !defined(USE_C11THREADS_)
#    include <unistd.h>
#    if defined(_POSIX_VERSION)
#      define USE_PTHREADS_ 1
#    endif
#  elif defined(_WIN32) && !defined(USE_C11THREADS_)
#    if defined(__WINPTHREADS_VERSION)
#      define USE_WINPTHREADS_ 1
#    endif
#  endif
#else
#  error "Possible non-Standard compliant implementation."
#endif

#if defined(USE_C11THREADS_)
#  include <threads.h>
#elif defined(USE_PTHREADS_) || defined(USE_WINPTHREADS_)
#  include <pthread.h>
#else
#  if defined(__GNUC__)
#    warning "llog couldn't find a proper locking mechanism. Its operations are NOT thread-safe."\
             "A locking mechanism can be provided through llog_set_lock."
#  elif defined(_MSC_VER)
#    pragma message("llog couldn't find a proper locking mechanism. Its operations are NOT thread-safe."\
                    "A locking mechanism can be provided through llog_set_lock.")
#  endif
#endif

#if defined(LLOG_COMPILE_WITH_DLL)  // Compiling with a shared library
#  if !defined(_WIN32) && defined(__GNUC__) && __GNUC__ >= 4
#    define LLOG_LOCAL __attribute__((visibility("hidden")))  // When compiled with a shared lib, only interface with the lib
#  endif
#else                               // Compiling with a static library
#  define LLOG_LOCAL
#endif

#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>
#include <stdlib.h>
#include <stdarg.h>

enum {
    LLOG_TRACE=0,
    LLOG_DEBUG,
    LLOG_INFO,
    LLOG_WARN,
    LLOG_ERROR,
    LLOG_FATAL
};

typedef struct {
    int level;               ///< Logging level
    unsigned long line;      ///< Line number
    const char *format;      ///< Format string
    const char *file;        ///< File name
    const char *func;        ///< Function name
    void *logobj;            ///< i.e. a file stream
    struct tm *time;         ///< Log time
    va_list args;            ///< Argument list provided to the format string
} llog_event;

typedef void (*llog_callback)(llog_event event);
typedef int (*llog_lock)(bool lockit /* or unlock it */, void *lockobj);

/**
 * @name Log macros.
 * @brief Log with trace, debug, info, warn, error, or fatal options.
 *
 * @remark Called with @a F as formatting string and its matching arguments.
 *
 * @retval 0 on success
 * @retval -ELOCK on locking/unlocking protocol failure
 */
///@{
#define llog_trace(...) _llog_with_context(LLOG_TRACE, _FIRST_ARG(__VA_ARGS__) "%.0d", _BUTFIRST_ARGS(__VA_ARGS__))
#define llog_debug(...) _llog_with_context(LLOG_DEBUG, _FIRST_ARG(__VA_ARGS__) "%.0d", _BUTFIRST_ARGS(__VA_ARGS__))
#define llog_info(...)  _llog_with_context(LLOG_INFO, _FIRST_ARG(__VA_ARGS__) "%.0d", _BUTFIRST_ARGS(__VA_ARGS__))
#define llog_warn(...)  _llog_with_context(LLOG_WARN, _FIRST_ARG(__VA_ARGS__) "%.0d", _BUTFIRST_ARGS(__VA_ARGS__))
#define llog_error(...) _llog_with_context(LLOG_ERROR, _FIRST_ARG(__VA_ARGS__) "%.0d", _BUTFIRST_ARGS(__VA_ARGS__))
#define llog_fatal(...) _llog_with_context(LLOG_FATAL, _FIRST_ARG(__VA_ARGS__) "%.0d", _BUTFIRST_ARGS(__VA_ARGS__))
///@}

/**
 * @brief If no default locking API was found this can be used to set a locking
 * mechanism.
 * @param lockfunc is a function with @a llog_lock prototype
 * @param lockobj is a locking object to be locked and unlocked as required
 *
 * @retval 0 on success
 * @retval -EINVAL if lockfunc or lockobj is null
 *
 * @warning Calling macros or functions of this module inside @a lockfunc will
 * result in undefined behavior.
 */
int llog_set_lock(llog_lock lockfunc, void *lockobj);

/**
 * @brief Enables quiet mode. When set to true, nothing is outputted to stderr.
 */
void llog_set_quiet(bool quiet);

/**
 * @brief Sets the log level. All logs below the provided @c level won't
 * be written to @c stderr.
 *
 * @param level Can be one of @c LLOG_TRACE, @c LLOG_DEBUG, @c LLOG_INFO,
 * @c LLOG_WARN, @c LLOG_ERROR, or @c LLOG_FATAL
 *
 * @retval 0 on success
 * @retval -EINVAL if @c level is an invalid value
 */
int llog_set_level(int level);

/**
 * @brief Adds a new callback function. The provided function can be callled with
 * the log data.
 *
 * @retval 0 on success
 * @retval -EINVAL if an invalid argument is passed
 * @retval -EOVERFLOW if the number of callbacks reached its maximum
 * @retval -ELOCK if an error occurred in the locking/unlocking mechanism
 *
 * @warning Calling macros or functions of this module inside @a logfunc
 * will result in undefined behavior.
 */
int llog_add_callback(llog_callback logfunc, void *logobj, int level);

/**
 * @brief Adds a new file pointer to which the log can be written.
 *
 * @return @see @c llog_add_callback
 */
int llog_add_fp(FILE *restrict fp, int level);

/*
 * These are not required by the Standard.
 *
 * The C Standard requires only EOF (negative), EDOM, EILSEQ, and ERANGE (all positive).
 */
#include <limits.h>
#include <errno.h>
#if !defined(EFAULT)
#  define EFAULT EDOM
#endif
#if !defined(EOVERFLOW)
#  define EOVERFLOW (EFAULT - EOF)
#  if (EOVERFLOW >= INT_MAX)
#    error "EOVERFLOW constant is too large."
#  endif
#endif
#if !defined(EINVAL)
#  define EINVAL (EOVERFLOW + EFAULT - EOF + EDOM)
#  if (EINVAL >= INT_MAX)
#    error "EINVAL constant is too large."
#  endif
#endif
#if !defined(ELOCK)
#  define ELOCK (EINVAL + EFAULT - EOF + EILSEQ)
#  if (ELOCK >= INT_MAX)
#    error "ELOCK constant is too large."
#  endif
#endif

/*
 * Extracts the first argument from __VA_ARGS__
 */
#define _FIRST_ARG(...) _FIRST_ARG_AUX(__VA_ARGS__, 0)
#define _FIRST_ARG_AUX(_first, ...) _first
/*
 * Removes the first argument from __VA_ARGS__
 */
#define _BUTFIRST_ARGS(...) _BUTFIRST_ARGS_AUX(__VA_ARGS__, 0)
#define _BUTFIRST_ARGS_AUX(_first, ...) __VA_ARGS__

#define _llog_with_context(LVL, F, ...) _llog_log(LVL, __FILE__, __func__, __LINE__+0UL, "" F "", __VA_ARGS__)

int _llog_log(int level, const char *restrict file, const char *restrict func,
              unsigned long line, const char *restrict format, ...);

/**
 * @warning This is NOT thread-safe.
 */
static inline void _llog_event_context(llog_event event[static 1], void *logobj)
{
    time_t now = time(0);
    event->time = localtime(&now);
    event->logobj = logobj;
}

#endif
