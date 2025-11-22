/* SPDX-License-Identifier: GPL-2.0-or-later */


#include <stdio.h>
#include <sys/times.h>
#include <unistd.h>
#include "builtins.h"

// Convert clock ticks to seconds (with hundredths precision)
static void print_time(clock_t ticks) {
    long clk_tck = sysconf(_SC_CLK_TCK);
    long seconds = ticks / clk_tck;
    long hundredths = (ticks * 100 / clk_tck) % 100;
    printf("%ldm%ld.%02lds", seconds / 60, seconds % 60, hundredths);
}

int builtin_times(char **argv) {
    (void)argv; // No arguments
    
    struct tms buf;
    times(&buf);
    
    // Print user and system time for shell
    print_time(buf.tms_utime);
    printf(" ");
    print_time(buf.tms_stime);
    printf("\n");
    
    // Print user and system time for children
    print_time(buf.tms_cutime);
    printf(" ");
    print_time(buf.tms_cstime);
    printf("\n");
    
    return 0;
}
