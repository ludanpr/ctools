/**
 * @file example.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "memstats.h"


int main(int argc, char *argv[])
{
    if (argc == 2) {
        if (strlen(argv[1]) != 2 || argv[1][0] != '-' || argv[1][1] != 'm') {
            fprintf(stderr, "Usage:\n%s [-m]\n", argv[0]);
            return EXIT_FAILURE;
        }
    }
    else if (argc > 2) {
        fprintf(stderr, "Usage:\n%s [-m]\n", argv[0]);
        return EXIT_FAILURE;
    }

    long long memused  = memstats_memused();
    long long mempeak  = memstats_mempeak();
    long long memfree  = memstats_memfree();
    long long memtotal = memstats_memtotal();
    char unit          = 'k';
    if (argc == 2) {
        memused  /= 1024LL;
        mempeak  /= 1024LL;
        memfree  /= 1024LL;
        memtotal /= 1024LL;
        unit      = 'm';
    }

    printf("MEMSTATS info:\n");
    printf("%lld %cB\n", memused, unit);
    printf("%lld %cB\n", mempeak, unit);
    printf("%lld %cB\n", memfree, unit);
    printf("%lld %cB\n", memtotal, unit);

    return EXIT_SUCCESS;
}
