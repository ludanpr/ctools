/*
 * C Header file: llog.h
 */
#ifndef _LLOG_GUARD_H_
#define _LLOG_GUARD_H_ 1

/**
 * @file
 * @brief An event log utility.
 *
 * This module is meant to be used in conjunction with another existing project.
 */

#if defined(__STDC_VERSION__)
#  if (__STDC_VERSION__ >= 201112L)
#    if !defined(__STDC_NO_THREADS__)
#      define _USE_C11THREADS_ 1
#    endif
#  endif
#  if defined(__unix__) && !defined(_USE_C11THREADS_)
#    include <unistd.h>
#    if defined(_POSIX_VERSION)
#      define _USE_PTHREADS_ 1
#    endif
#  elif defined(_WIN32) && !defined(_USE_C11THREADS_)
#    if defined(__WINPTHREADS_VERSION)
#      define _USE_WINPTHREADS_ 1
#    endif
#  endif
#else
#  error "Possible non-Standard compliant implementation."
#endif

#if defined(_USE_C11THREADS_)
#  include <threads.h>
#elif defined(_USE_PTHREADS_) || defined(_USE_WINPTHREADS_)
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
    struct tm *time;         ///< Logging time
    va_list args;            ///< Argument list provided to the format string
} llog_event;

typedef void (*llog_callback)(llog_event event);
typedef void (*llog_lock)(bool lockit /* or unlock it */, void *lockobj);

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
#define llog_trace(...) _llog_trace(_FIRST_ARG(__VA_ARGS__) "%.0d", _BUTFIRST_ARGS(__VA_ARGS__))
#define llog_debug(...) _llog_debug(_FIRST_ARG(__VA_ARGS__) "%.0d", _BUTFIRST_ARGS(__VA_ARGS__))
#define llog_info(...)  _llog_info(_FIRST_ARG(__VA_ARGS__) "%.0d", _BUTFIRST_ARGS(__VA_ARGS__))
#define llog_warn(...)  _llog_warn(_FIRST_ARG(__VA_ARGS__) "%.0d", _BUTFIRST_ARGS(__VA_ARGS__))
#define llog_error(...) _llog_error(_FIRST_ARG(__VA_ARGS__) "%.0d", _BUTFIRST_ARGS(__VA_ARGS__))
#define llog_fatal(...) _llog_fatal(_FIRST_ARG(__VA_ARGS__) "%.0d", _BUTFIRST_ARGS(__VA_ARGS__))
///@}

/**
 * @brief If no default locking API was found this can be used to set a locking
 * mechanism.
 * @param lockfunc is a function with @a llog_lock prototype
 * @param lockobj is a locking object to be locked and unlocked as required
 */
void llog_set_lock(llog_lock lockfunc, void *lockobj);

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
 */
int llog_add_callback(llog_callback logfunc, void *logobj, int level);

/**
 * @brief Adds a new file pointer to which the log can be written.
 *
 * @return @see @c llog_add_callback
 */
int llog_add_fp(FILE *fp, int level);

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

#define _llog_trace(F, ...) _llog_log(LLOG_TRACE, __FILE__, __func__, __LINE__+0UL, "" F "", __VA_ARGS__)
#define _llog_debug(F, ...) _llog_log(LLOG_DEBUG, __FILE__, __func__, __LINE__+0UL, "" F "", __VA_ARGS__)
#define _llog_info(F, ...)  _llog_log(LLOG_INFO, __FILE__, __func__, __LINE__+0UL, "" F "", __VA_ARGS__)
#define _llog_warn(F, ...)  _llog_log(LLOG_WARN, __FILE__, __func__, __LINE__+0UL, "" F "",__VA_ARGS__)
#define _llog_error(F, ...) _llog_log(LLOG_ERROR, __FILE__, __func__, __LINE__+0UL, "" F "", __VA_ARGS__)
#define _llog_fatal(F, ...) _llog_log(LLOG_FATAL, __FILE__, __func__, __LINE__+0UL, "" F "", __VA_ARGS__)

int _llog_log(int level, const char *restrict file, const char *restrict func,
              unsigned long line, const char *restrict format, ...);

/**
 * @warning Do not call this function directly. This is NOT thread-safe.
 */
static inline void _llog_event_context(llog_event event[static 1], void *logobj)
{
    time_t now = time(0);
    event->time = localtime(&now);
    event->logobj = logobj;
}

#endif
