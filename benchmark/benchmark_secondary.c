/*
 * Copyright 2022, UNSW
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <stdint.h>
#include <microkit.h>
#include <sel4/benchmark_track_types.h>
#include <sel4/benchmark_utilisation_types.h>
#include <sddf/benchmark/bench.h>
#include <sddf/benchmark/sel4bench.h>
#include <sddf/benchmark/util.h>
#include <sddf/util/fence.h>
#include <sddf/util/util.h>
#include <sddf/util/printf.h>

#define LOG_BUFFER_CAP 7

uintptr_t uart_base;

ccnt_t counter_values[8];
counter_bitfield_t benchmark_bf;
uint16_t core;
uint64_t core_clients;

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

            if (core != 4) microkit_notify(TX_START);
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

            THREAD_MEMORY_RELEASE();
            if (core != 4) microkit_notify(TX_STOP);
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
}