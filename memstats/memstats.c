/*
 *  Copyright (c) 2011-2019, Triad National Security, LLC.
 *  All rights Reserved.
 *
 *  CLAMR -- LA-CC-11-094
 *
 *  Copyright 2011-2019. Triad National Security, LLC. This software was produced
 *  under U.S. Government contract 89233218CNA000001 for Los Alamos National
 *  Laboratory (LANL), which is operated by Triad National Security, LLC
 *  for the U.S. Department of Energy. The U.S. Government has rights to use,
 *  reproduce, and distribute this software.  NEITHER THE GOVERNMENT NOR
 *  TRIAD NATIONAL SECURITY, LLC MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR
 *  ASSUMES ANY LIABILITY FOR THE USE OF THIS SOFTWARE.  If software is modified
 *  to produce derivative works, such modified software should be clearly marked,
 *  so as not to confuse it with the version available from LANL.
 *
 *  Additionally, redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Triad National Security, LLC, Los Alamos
 *       National Laboratory, LANL, the U.S. Government, nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE TRIAD NATIONAL SECURITY, LLC AND
 *  CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT
 *  NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL TRIAD NATIONAL
 *  SECURITY, LLC OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 *  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 *  OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 *  CLAMR -- LA-CC-11-094
 *  This research code is being developed as part of the
 *  2011 X Division Summer Workshop for the express purpose
 *  of a collaborative code for development of ideas in
 *  the implementation of AMR codes for Exascale platforms
 *
 *  AMR implementation of the Wave code previously developed
 *  as a demonstration code for regular grids on Exascale platforms
 *  as part of the Supercomputing Challenge and Los Alamos
 *  National Laboratory
 *
 *  Authors: Bob Robey       XCP-2   brobey@lanl.gov
 *           Neal Davis              davis68@lanl.gov, davis68@illinois.edu
 *           David Nicholaeff        dnic@lanl.gov, mtrxknight@aol.com
 *           Dennis Trujillo         dptrujillo@lanl.gov, dptru10@gmail.com
 *
 */
#include <sys/time.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <unistd.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef __APPLE_CC__
#include <mach/mach_host.h>
#include <mach/task.h>
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "memstats.h"

pid_t pid;
FILE *statfp    = (void *) 0;
FILE *meminfofp = (void *) 0;

long long memstats_memused(void)
{
    long long memcurrent = 0LL;

#ifdef __APPLE_CC__
    /*
     * This is all memory used and we want memory for only our process -- do
     * alternate
     * vm_size_t page_size;
     * mach_port_t mach_port;
     * mach_msg_type_number_t count = HOST_VM_INFO_COUNT;
     *
     * host_page_size(mach_port, &page_size);
     * vm_statistics_data_t vmstat;
     * host_statistics(mach_host_self(), HOST_VM_INFO, (host_info_t) &vmstat, &count);
     *
     * memcurrent = (vmstat.wire_count + vmstat.active_count + vmstat.inactive_count) * page_size / 1024;
     */
    struct task_basic_info tinfo;
    mach_msg_type_number_t tinfocount = TASK_BASIC_INFO_COUNT;
    task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t) &tinfo, &tinfocount);

    memcurrent = tinfo.resident_size;
#else
    char procstatfile[50];
    int memdebug = 0;

    if (!statfp) {
        pid = getpid();
        sprintf(procstatfile, "/proc/%d/status", pid);
        statfp = fopen(procstatfile, "r");
        if (!statfp) {
            return -1LL;
        }
    }

    int err = fflush(statfp);
    if (err) {
        fprintf(stderr, "fflush %s failed: %s\n", procstatfile, strerror(err));
        return -1LL;
    }

    err = fseek(statfp, 0L, 0);
    if (err) {
        fprintf(stderr, "fseek %s failed: %s\n", procstatfile, strerror(err));
        return -1LL;
    }

    char *str = malloc(140 * sizeof *str);
    while (!feof(statfp)) {
        str = fgets(str, 132, statfp);
        if (!str) fprintf(stderr, "Warning: Error in reading %s for memory stats\n", procstatfile);

        char *p = strtok(str, ":");
        if (!strcmp(p, "VmRSS")) {
            p = strtok('\0', " ");
            p = strtok('\0', " ");

            memcurrent = strtoll(p, (void *) 0, 10); // * 1024
            if (memdebug) printf("VmRSS %lld\n", memcurrent);
            break;
        }
    }
    free(str);

    fclose(statfp);
    statfp = (void *) 0;
#endif

    return memcurrent;
}

long long memstats_mempeak(void)
{
    long long memcurrent = 0LL;
    char procstatfile[50];
    int memdebug = 0;

    if (!statfp) {
        pid = getpid();
        sprintf(procstatfile, "/proc/%d/status", pid);
        statfp = fopen(procstatfile, "r");
        if (!statfp) {
            return -1LL;
        }
    }

    int err = fflush(statfp);
    if (err) {
        fprintf(stderr, "fflush %s failed: %s\n", procstatfile, strerror(err));
        return -1LL;
    }

    err = fseek(statfp, 0L, 0);
    if (err) {
        fprintf(stderr, "fseek %s failed: %s\n", procstatfile, strerror(err));
        return -1LL;
    }

    char *str = malloc(140 * sizeof *str);
    while (!feof(statfp)) {
        str = fgets(str, 132, statfp);
        if (!str) fprintf(stderr, "Warning: Error in reading %s for memory stats\n", procstatfile);

        char *p = strtok(str, ":");
        if (!strcmp(p, "VmHWM")) {
            p = strtok('\0', " ");
            p = strtok('\0', " ");

            memcurrent = strtoll(p, (void *) 0, 10);
            if (memdebug) printf("VmHWM %lld\n", memcurrent);
            break;
        }
    }
    free(str);

    fclose(statfp);
    statfp = (void *) 0;

    return memcurrent;
}

#define TIMER_ONEK 1024

long long memstats_memfree(void)
{
    long long freemem = 0LL;

#ifdef __APPLE_CC__
    mach_msg_type_number_t count = HOST_VM_INFO_COUNT;
    mach_port_t machport         = mach_host_self();

    vm_size_t pagesize;
    host_page_size(machport, &pagesize);
    vm_statistics64_data_t vmstat;
    host_statistics64(machport, HOST_VM_INFO, (host_info_t) &vmstat, &count);

    freemem = vmstat.free_count * pagesize / 1024;
#else
    char buf[260];
    int memdebug = 0;

    freemem = -1LL;

    if (!meminfofp) {
        meminfofp = fopen("/proc/meminfo", "r");
        if (!meminfofp) {
            fprintf(stderr, "fopen failed: \n");
            return -1LL;
        }
    }

    int err = fflush(meminfofp);
    if (err) {
        fprintf(stderr, "fflush failed: %s\n", strerror(err));
        return -1LL;
    }

    err = fseek(meminfofp, 0L, 0);
    if (err) {
        fprintf(stderr, "fseek failed: %s\n", strerror(err));
        return -1LL;
    }

    char *p;
    while (!feof(meminfofp)) {
        if (fgets(buf, 255, meminfofp)) {
            p = strtok(buf, ":");
            if (memdebug) printf("p: |%s|\n", p);
            if (!strcmp(p, "MemFree")) {
                p = strtok('\0', " ");
                freemem = strtoll(p, (void *) 0, 10);
                break;
            }
        }
    }

    fclose(meminfofp);
    meminfofp = (void *) 0;
#endif

    return freemem;
}

long long memstats_memtotal(void)
{
    long long totalmem = 0LL;

#ifdef __APPLE_CC__
    int mib[2] = { [0] = CTL_HW, [1] = HW_MEMSIZE };
    size_t length = sizeof totalmem;
    sysctl(mib, 2, &totalmem, &length, (void *) 0, 0);
    totalmem /= 1024;
#else
    char buf[260];
    int memdebug = 0;

    totalmem = -1LL;

    if (!meminfofp) {
        meminfofp = fopen("/proc/meminfo", "r");
        if (!meminfofp) {
            fprintf(stderr, "fopen failed: \n");
            return -1LL;
        }
    }

    int err = fflush(meminfofp);
    if (err) {
        fprintf(stderr, "fflush failed: %s\n", strerror(err));
        return -1LL;
    }

    err = fseek(meminfofp, 0L, 0);
    if (err) {
        fprintf(stderr, "fseek failed: %s\n", strerror(err));
        return -1LL;
    }

    char *p;
    while (!feof(meminfofp)) {
        if (fgets(buf, 255, meminfofp)) {
            p = strtok(buf, ":");
            if (memdebug) printf("p: |%s|\n", p);
            if (!strcmp(p, "MemTotal")) {
                p = strtok('\0', " ");
                totalmem = strtoll(p, (void *) 0, 10);
                break;
            }
        }
    }

    fclose(meminfofp);
    meminfofp = (void *) 0;
#endif

    return totalmem;
}
