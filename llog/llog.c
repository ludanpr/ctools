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
#if !defined(CACHELINE_SIZE)
#  define CACHELINE_SIZE 64U
#endif

#if __STDC_VERSION__ < 201112L
typedef unsigned char cacheline_padding_[CACHELINE_SIZE];
#endif

static struct {
#if __STDC_VERSION__ >= 201112L
    _Alignas(CACHELINE_SIZE) size_t cbidx;
    int level;
#  if defined(USE_C11THREADS_)
    _Alignas(CACHELINE_SIZE) mtx_t mutex;
#  elif defined(USE_PTHREADS_) || defined(USE_WINPTHREADS_)
    _Alignas(CACHELINE_SIZE) pthread_mutex_t mutex;
#  else
    _Alignas(CACHELINE_SIZE) void *lockobj;
    llog_lock lockfunc;
#  endif
    _Alignas(CACHELINE_SIZE) callback cbs[LLOG_MAX_CBS];
    _Alignas(CACHELINE_SIZE) bool quiet;

#else
    cacheline_padding_ padding0;
    size_t cbidx;
    int level;
    cacheline_padding_ padding1;
#  if defined(USE_C11THREADS_)
    mtx_t mutex;
#  elif defined(USE_PTHREADS_) || defined(USE_WINPTHREADS_)
    pthread_mutex_t mutex;
#  else
    void *lockobj;
    llog_lock lockfunc;
#  endif
    cacheline_padding_ padding2;
    callback cbs[LLOG_MAX_CBS];
    cacheline_padding_ padding3;
    bool quiet;
    cacheline_padding_ padding4;
#endif
} _llog = { .cbidx = 0,
            .level = LLOG_TRACE,
#if defined(USE_PTHREADS_) || defined(USE_WINPTHREADS_)
            .mutex = PTHREAD_MUTEX_INITIALIZER,
#elif !defined(USE_C11THREADS_)
            .lockfunc = (void *) 0,
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
#if defined(USE_C11THREADS_)
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

LLOG_LOCAL
int llog_set_lock(llog_lock lockfunc, void *lockobj)
{
    return 0;
}

/*------------------------------------------------------------------------------------------------------------*/
#elif defined(USE_PTHREADS_) || defined(USE_WINPTHREADS_)

LLOG_LOCAL
int llog_set_lock(llog_lock lockfunc, void *lockobj)
{
    return 0;
}

/*------------------------------------------------------------------------------------------------------------*/
#else

LLOG_LOCAL
int llog_set_lock(llog_lock lockfunc, void *lockobj)
{
    if (!lockfunc || !lockobj) return -EINVAL;

    _llog.lockfunc = lockfunc;
    _llog.lockobj = lockobj;
    return 0;
}

/*-------------------------------------------------------------------------------------------------------------*/
#endif

static int _lock(void)
{
#if defined(USE_C11THREADS_)
    call_once(&flag, _llog_mtx_init);

    int status = mtx_lock(&_llog.mutex);
    if (status != thrd_success) return -ELOCK;
#elif defined(USE_PTHREADS_) || defined(USE_WINPTHREADS_)
    int status = pthread_mutex_lock(&_llog.mutex);
    if (status) return -ELOCK;
#else
    if (_llog.lockfunc) {
        return _llog.lockfunc(true, _llog.lockobj);
    }
#endif
    return 0;
}

static int _unlock(void)
{
#if defined(USE_C11THREADS_)
    int status = mtx_unlock(&_llog.mutex);
    if (status != thrd_success) return -ELOCK;
#elif defined(USE_PTHREADS_) || defined(USE_WINPTHREADS_)
    int status = pthread_mutex_unlock(&_llog.mutex);
    if (status) return -ELOCK;
#else
    if (_llog.lockfunc) {
        return _llog.lockfunc(false, _llog.lockobj);
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

LLOG_LOCAL
void llog_set_quiet(bool quiet)
{
    _llog.quiet = quiet;
}

LLOG_LOCAL
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
LLOG_LOCAL
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

LLOG_LOCAL
int llog_add_fp(FILE *restrict fp, int level)
{
    return llog_add_callback(_file_callback, fp, level);
}

#if defined(__GNUC__)
__attribute__((format(printf, 5, 6)))
#endif
LLOG_LOCAL
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
