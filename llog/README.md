[![Built with Spacemacs](https://cdn.rawgit.com/syl20bnr/spacemacs/442d025779da2f62fc86c2082703697714db6514/assets/spacemacs-badge.svg)](http://spacemacs.org)

# llog - log utility

This module provides a log utility with `printf`-like interfaces equivalent to:

``` c
int llog_trace(const char *restrict format, ...);
int llog_debug(const char *restrict format, ...);
int llog_info(const char *restrict format, ...);
int llog_warn(const char *restrict format, ...);
int llog_error(const char *restrict format, ...);
int llog_fatal(const char *restrict format, ...);
```

<b>Note</b> that it is required that `format` be a string literal.
If defined, the macro `LLOG_COLOR` will cause the output to `stderr` to be colored.

## Setting a lock (if C11 threads nor POSIX threads are present)
By default, this module provides out of the box locking for its log operations. This mechanism
is provided by default if C11 threads or POSIX threads are present. If this is not the case, a
locking mechanism can be provided through:

```c
int llog_set_lock(llog_lock lockfunc, void *lockobj);
```

<b>WARNING</b>: Calling any of the macros or functions of this module inside `lockfunc` will result
in undefined behavior.

An example using POSIX threads is given below (NOTE again that this is only illustrative, if POSIX
threads are present, the locking mechanism is supposed to be in place already).

``` c
#include "llog.h"
#include <stdbool.h>
#include <pthread.h>

int lock_unlock(bool lockit, void *mutex)
{
    pthread_mutex_t *mtx = (pthread_mutex_t *) mutex;
    if (lockit) {
        int status = pthread_mutex_lock(mtx);
        if (status) {
            return -ELOCK;
        }
    }
    else {
        int status = pthread_mutex_unlock(mtx);
        if (status) {
            return -ELOCK;
        }
    }
}

int main(void)
{
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    llog_set_lock(lock_unlock, &mutex);
    /* ... */
    
    return 0;
}
```

## Quiet mode
Does not output to `stderr` (not the default behavior) if `quiet` is set to `true`.

```c
void llog_set_quiet(bool quiet);
```

## Set log level
This function sets the level of log. One of (in order) `LLOG_TRACE`, `LLOG_DEBUG`, `LLOG_INFO`, `LLOG_WARN`,
`LLOG_ERROR`, `LLOG_FATAL`, i.e., if `LLOG_INFO` is set, <i>traces</i> and <i>debugs</i> will be ignored. 

```c
int llog_set_level(int level);
```

## Adding callbacks and file pointers to write to
This functions adds, respectively, a callback function with the interface `void logfunc(llog_event event);`
that can be used to log.

<b>WARNING</b>: Calling any macro or function of this module in `logfunc` will result in undefined behavior.

```c
int llog_add_callback(llog_callback logfunc, void *logobj, int level);
int llog_add_fp(FILE *restrict fp, int level);
```

## Visibility with shared libraries
When compiled with a shared (dynamic) library, it is possible to change the default visibility of the interface
in linux, for example, where the interface is completely visible by default. This can be set in a GNU compiler
that implements visibility attributes by defining a macro with `-DLLOG_COMPILE_WITH_DLL`. Given this, the shared
library linker symbols for LLOG will be hidden from the "outside world".
