#include <stdio.h>
#include <stdlib.h>
#include "llog.h"

void *function(void)
{
    double *arr = malloc(-1 * sizeof(double[100]));
    if (!arr) {
        llog_error("malloc failed");
        return (void *) 0;
    }
    return arr;
}


int main(void)
{
    /*
     * Simple example for visualization. Compile with -DLLOG_COLOR (gcc)
     * to see colored messages
     */
    llog_trace("this is a trace: %d", 1);
    llog_debug("this is a debug: %d", 2);
    llog_info("this is info: %d", 3);
    llog_warn("this is a warn: %d", 4);
    llog_error("this is an error: %d", 5);
    llog_fatal("this is a fatal error: %d", 6);

    llog_trace("only a trace message");
    llog_debug("only a debug message");
    llog_info("only a info message");
    llog_warn("only a warning message");
    llog_error("only a error message");
    llog_fatal("only a fatal message");

    /*
     * This can't be done. It is a feature to permit only string literals.
     */
    //char *fmt = "A format string: %d";
    //llog_trace(fmt, 10);

    /*
     * Not ignoring error.
     */
    int status = llog_warn("This doesn't just ignore errors");
    if (status) {
        /* Handler code here */
    }

    status = llog_add_fp(0, LLOG_WARN);
    if (status == -EINVAL) llog_error("Error code (%d): called with invalid argument", -EINVAL);

    /*
     * Logging to simple files.
     */
    FILE *fp1 = fopen("logfile0.log", "a");
    if (!fp1) {
        llog_error("failed to open file");
        return EXIT_FAILURE;
    }
    FILE *fp2 = fopen("logfil1.log", "a");
    if (!fp2) {
        llog_error("failed to open file");
    }

    llog_add_fp(fp1, LLOG_TRACE);
    llog_add_fp(fp2, LLOG_TRACE);

    double *ptr = function();
    if (!ptr) {
        llog_fatal("pointer at %p shouldn't be null", (void *) &ptr);
        return EXIT_FAILURE;
    }
    fclose(fp1);
    fclose(fp2);

    llog_set_quiet(true); // don't output to stderr
    /* ... more logs */

    return EXIT_SUCCESS;
}
