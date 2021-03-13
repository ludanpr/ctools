/*
 * Implementation of llog
 */
#include "llog.h"

typedef struct {
    int level;
    void *logobj;
    llog_callback cbfunc;
} callback;

#define LLOG_MAX_CBS 63U

static struct {
    size_t cbidx;
    int level;
#if defined(_USE_C11THREADS_)
    mtx_t mutex;
#elif defined(_USE_PTHREADS_) || defined(_USE_WINPTHREADS_)
    pthread_mutex_t mutex;
#else
    void *lockobj;
    llog_lock lockfunc;
#endif
    callback cbs[LLOG_MAX_CBS];
    bool quiet;
} _llog = { .cbidx = 0,
            .level = LLOG_TRACE,
#if defined(_USE_PTHREADS_) || defined(_USE_WINPTHREADS_)
            .mutex = PTHREAD_MUTEX_INITIALIZER,
#endif
};

#define LLEVEL_STR                                         \
    (const char *const[]){                                 \
    "TRACE", "DEBUG", "INFO", "WARNING", "ERROR", "FATAL", \
    }

#if defined(LLOG_COLOR)
#define LLEVEL_COLOR                                                        \
    (const char *const[]){                                                  \
    "\x1b[94m", "\x1b[36m", "\x1b[32m", "\x1b[33m", "\x1b[31m", "\x1b[35m", \
    }
#endif

void _llog_event_context(llog_event [static 1], void *);

/*------------------------------------------------------------------------------------------------------------*/
#if defined(_USE_C11THREADS_)
static void _llog_mtx_destroy(void)
{
    mtx_destroy(&_llog.mutex);
}

static void _llog_mtx_init(void)
{
    int status = mtx_init(&_llog.mutex, mtx_plain);
    if (status != thrd_success) {
        fprintf(stderr, "Failed to initialize mutex. Exiting...\n");
        exit(EXIT_FAILURE);
    }
    atexit(_llog_mtx_destroy);
}

static once_flag flag = ONCE_FLAG_INIT;
static void _llog_mtx_init_once(void)
{
    call_once(&flag, _llog_mtx_init);
}

void llog_set_lock(llog_lock lockfunc, void *lockobj)
{
    /* Do nothing */
}

/*------------------------------------------------------------------------------------------------------------*/
#elif defined(_USE_PTHREADS_) || defined(_USE_WINPTHREADS_)

void llog_set_lock(llog_lock lockfunc, void *lockobj)
{
    /* Do nothing */
}

/*------------------------------------------------------------------------------------------------------------*/
#else

void llog_set_lock(llog_lock lockfunc, void *lockobj)
{
    _llog.lockfunc = lockfunc;
    _llog.lockobj = lockobj;
}

/*-------------------------------------------------------------------------------------------------------------*/
#endif

static int _lock(void)
{
#if defined(_USE_C11THREADS_)
    _llog_mtx_init_once();

    int status = mtx_lock(&_llog.mutex);
    if (status != thrd_success) return -ELOCK;
#elif defined(_USE_PTHREADS_) || defined(_USE_WINPTHREADS_)
    int status = pthread_mutex_lock(&_llog.mutex);
    if (status) return -ELOCK;
#else
    if (_llog.lockfunc) {
        _llog.lockfunc(true, _llog.lockobj);
    }
#endif
    return 0;
}

static int _unlock(void)
{
#if defined(_USE_C11THREADS_)
    int status = mtx_unlock(&_llog.mutex);
    if (status != thrd_success) return -ELOCK;
#elif defined(_USE_PTHREADS_) || defined(_USE_WINPTHREADS_)
    int status = pthread_mutex_unlock(&_llog.mutex);
    if (status) return -ELOCK;
#else
    if (_llog.lockfunc) {
        _llog.lockfunc(false, _llog.lockobj);
    }
#endif
    return 0;
}

static void _stdout_callback(llog_event event)
{
    char datefmt[21];
    datefmt[strftime(datefmt, sizeof datefmt, "%T", event.time)] = 0;

#if defined(LLOG_COLOR)
    fprintf(event.logobj, "%s %s%-7s\x1b[0m \x1b[90m[%s]:%s:%lu:\x1b[0m ", datefmt,
            LLEVEL_COLOR[event.level], LLEVEL_STR[event.level], event.file, event.func, event.line);
#else
    fprintf(event.logobj, "%s %-7s [%s]:%s:%lu: ", datefmt, LLEVEL_STR[event.level],
            event.file, event.func, event.line);
#endif

    vfprintf(event.logobj, event.format, event.args);
    fputs("\n", event.logobj);
    fflush(event.logobj);
}

static void _file_callback(llog_event event)
{
    char datefmt[64];
    datefmt[strftime(datefmt, sizeof datefmt, "%Y-%m-%d %T", event.time)] = 0;

    fprintf(event.logobj, "%s %-7s [%s]:%s:%lu: ", datefmt, LLEVEL_STR[event.level],
            event.file, event.func, event.line);
    vfprintf(event.logobj, event.format, event.args);
    fputs("\n", event.logobj);
    fflush(event.logobj);
}

void llog_set_quiet(bool quiet)
{
    _llog.quiet = quiet;
}

int llog_set_level(int level)
{
    switch(level) {
    case LLOG_TRACE:
    case LLOG_DEBUG:
    case LLOG_INFO:
    case LLOG_WARN:
    case LLOG_ERROR:
    case LLOG_FATAL:
        _llog.level = level;
        return 0;
    default:
        return -EINVAL;
    }
}

// TODO: expose an interface to remove callbacks and file pointers.
int llog_add_callback(llog_callback logfunc, void *logobj, int level)
{
    if (!logfunc) return -EINVAL;
    if (!logobj) return -EINVAL;
    switch(level) {
    default:
        return -EINVAL;
    case LLOG_TRACE:
    case LLOG_DEBUG:
    case LLOG_INFO:
    case LLOG_WARN:
    case LLOG_ERROR:
    case LLOG_FATAL:
        break;
    }

    int status = _lock();
    if (status) return status;

    if (_llog.cbidx == LLOG_MAX_CBS) {
        status = _unlock();
        if (status) return status;   /* an error in unlock has precedence here. */
        return -EOVERFLOW;
    }

    _llog.cbs[_llog.cbidx++] = (callback){
        .cbfunc = logfunc,
        .level = level,
        .logobj = logobj,
    };

    status = _unlock();
    if (status) return status;

    return 0;
}

int llog_add_fp(FILE *restrict fp, int level)
{
    return llog_add_callback(_file_callback, fp, level);
}

#if defined(__GNUC__)
__attribute__((format(printf, 5, 6)))
#endif
int _llog_log(int level, const char *restrict file, const char *restrict func,
              unsigned long line, const char *restrict format, ...)
{
    llog_event event = { .level = level, .file = file, .func = func, .line = line, .format = format, };

    int status = _lock();
    if (status) return status;

    if (!_llog.quiet && _llog.level <= level) {
        _llog_event_context(&event, stderr);
        va_start(event.args, format);
        _stdout_callback(event);
        va_end(event.args);
    }
    for (size_t i = 0; i < _llog.cbidx; i++) {
        callback cb = _llog.cbs[i];
        if (cb.level <= level) {
            _llog_event_context(&event, cb.logobj);
            va_start(event.args, format);
            cb.cbfunc(event);
            va_end(event.args);
        }
    }

    status = _unlock();
    if (status) return status;
    return 0;
}
