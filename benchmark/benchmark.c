/*
 * Copyright 2022, UNSW
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <stdint.h>
#include <microkit.h>
#include <sddf/benchmark/bench.h>
#include <sddf/benchmark/util.h>
#include <sddf/util/fence.h>
#include <sddf/util/util.h>
#include <sddf/util/printf.h>

#define LOG_BUFFER_CAP 7
#define INIT 5

uintptr_t uart_base;

ccnt_t counter_values[8];
counter_bitfield_t benchmark_bf;
uint16_t core;
uint64_t core_clients;

#ifdef CONFIG_BENCHMARK_TRACK_KERNEL_ENTRIES
benchmark_track_kernel_entry_t *log_buffer;

static inline void seL4_BenchmarkTrackDumpSummary(benchmark_track_kernel_entry_t *logBuffer, uint64_t logSize)
{
    seL4_Word index = 0;
    seL4_Word syscall_entries = 0;
    seL4_Word fastpaths = 0;
    seL4_Word interrupt_entries = 0;
    seL4_Word userlevelfault_entries = 0;
    seL4_Word vmfault_entries = 0;
    seL4_Word debug_fault = 0;
    seL4_Word other = 0;

    while (logBuffer[index].start_time != 0 && index < logSize) {
        if (logBuffer[index].entry.path == Entry_Syscall) {
            if (logBuffer[index].entry.is_fastpath) fastpaths++;
            syscall_entries++;
        } else if (logBuffer[index].entry.path == Entry_Interrupt) interrupt_entries++;
        else if (logBuffer[index].entry.path == Entry_UserLevelFault) userlevelfault_entries++;
        else if (logBuffer[index].entry.path == Entry_VMFault) vmfault_entries++;
        else if (logBuffer[index].entry.path == Entry_DebugFault) debug_fault++;
        else other++;

        index++;
    }

    printf("Number of system call invocations  %llx and fastpaths  %llx\n", syscall_entries, fastpaths);
    printf("Number of interrupt invocations  %llx\n", interrupt_entries);
    printf("Number of user-level faults  %llx\n", userlevelfault_entries);
    printf("Number of VM faults  %llx\n", vmfault_entries);
    printf("Number of debug faults  %llx\n", debug_fault);
    printf("Number of others  %llx\n", other);
}
#endif

void notified(microkit_channel ch)
{
    switch(ch) {
        case RX_START:
            sel4bench_reset_counters();
            THREAD_MEMORY_RELEASE();
            sel4bench_start_counters(benchmark_bf);

            #ifdef CONFIG_BENCHMARK_TRACK_UTILISATION
            microkit_benchmark_start(core_clients);
            #endif

            #ifdef CONFIG_BENCHMARK_TRACK_KERNEL_ENTRIES
            seL4_BenchmarkResetLog();
            #endif

            if (core != 3) microkit_notify(TX_START);
            break;
        case RX_STOP:
            sel4bench_get_counters(benchmark_bf, &counter_values[0]);
            sel4bench_stop_counters(benchmark_bf);

            printf("{CORE %u: \n", core);
            for (int i = 0; i < ARRAY_SIZE(benchmarking_events); i++) printf("%s: %llX\n", counter_names[i], counter_values[i]);
            printf("}\n");

            #ifdef CONFIG_BENCHMARK_TRACK_UTILISATION
            microkit_benchmark_stop_all(core_clients);
            #endif

            #ifdef CONFIG_BENCHMARK_TRACK_KERNEL_ENTRIES
            entries = seL4_BenchmarkFinalizeLog();
            printf("KernelEntries:  %llx\n", entries);
            seL4_BenchmarkTrackDumpSummary(log_buffer, entries);
            #endif

            THREAD_MEMORY_RELEASE();
            if (core != 3) microkit_notify(TX_STOP);
            break;
        default:
            dprintf("Bench thread notified on unexpected channel %llu\n", ch);
    }
}

void init(void)
{
    sel4bench_init();
    benchmark_init_sys(microkit_name, &core, &core_clients);
    seL4_Word n_counters = sel4bench_get_num_counters();

    counter_bitfield_t mask = 0;
    for (seL4_Word counter = 0; counter < n_counters; counter++) {
        if (counter >= ARRAY_SIZE(benchmarking_events)) break;
        sel4bench_set_count_event(counter, benchmarking_events[counter]);
        mask |= BIT(counter);
    }

    sel4bench_reset_counters();
    sel4bench_start_counters(mask);
    benchmark_bf = mask;

    /* Notify the idle thread that the sel4bench library is initialised. */
    microkit_notify(INIT);

#ifdef CONFIG_BENCHMARK_TRACK_KERNEL_ENTRIES
    int res_buf = seL4_BenchmarkSetLogBuffer(LOG_BUFFER_CAP);
    if (res_buf) printf("Could not set log buffer:  %llx\n", res_buf);
    else printf("Log buffer set\n");
#endif
}