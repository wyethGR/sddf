#include <sel4/benchmark_track_types.h>
#include <sel4/benchmark_utilisation_types.h>
#include <sddf/benchmark/sel4bench.h>
#include <sddf/util/printf.h>
#include <ethernet_config.h>

#define RX_START 1
#define RX_STOP 2
#define TX_START 3
#define TX_STOP 4

char *counter_names[] = {
    "L1 i-cache misses",
    "L1 d-cache misses",
    "L1 i-tlb misses",
    "L1 d-tlb misses",
    "Instructions",
    "Branch mispredictions",
};

event_id_t benchmarking_events[] = {
    SEL4BENCH_EVENT_CACHE_L1I_MISS,
    SEL4BENCH_EVENT_CACHE_L1D_MISS,
    SEL4BENCH_EVENT_TLB_L1I_MISS,
    SEL4BENCH_EVENT_TLB_L1D_MISS,
    SEL4BENCH_EVENT_EXECUTE_INSTRUCTION,
    SEL4BENCH_EVENT_BRANCH_MISPREDICT,
};

static void print_benchmark_details(uint64_t pd_id, uint64_t kernel_util, uint64_t kernel_entries, uint64_t number_schedules, uint64_t total_util)
{
    if (pd_id == PD_TOTAL) printf("Total utilisation details: \n");
    else {
        printf("Utilisation details for PD: ");
        switch (pd_id) {
            case PD_ETH_ID: printf(DRIVER_NAME); break;
            case PD_MUX_RX_ID: printf(MUX_RX_NAME); break;
            case PD_MUX_TX_ID: printf(MUX_TX_NAME); break;
            case PD_COPY_ID: printf(COPY0_NAME); break;
            case PD_COPY1_ID: printf(COPY1_NAME); break;
            case PD_LWIP_ID: printf(CLI0_NAME); break;
            case PD_LWIP1_ID: printf(CLI1_NAME); break;
            case PD_ARP_ID: printf(ARP_NAME); break;
            case PD_TIMER_ID: printf("timer"); break;
        }
        printf(" (%llx)\n", pd_id);
    }
    printf("{\nKernelUtilisation:  %llx\nKernelEntries:  %llx\nNumberSchedules:  %llx\nTotalUtilisation:  %llx\n}\n", 
            kernel_util, kernel_entries, number_schedules, total_util);
}

#ifdef CONFIG_BENCHMARK_TRACK_UTILISATION

static void microkit_benchmark_start(uint64_t core_clients)
{
    seL4_BenchmarkResetThreadUtilisation(TCB_CAP);
    for (uint64_t pd_id = 1; pd_id < 64; pd_id++) {
        if ((core_clients >> pd_id) & 1) {
            seL4_BenchmarkResetThreadUtilisation(BASE_TCB_CAP + pd_id);
        }
    }
    seL4_BenchmarkResetLog();
}

static void microkit_benchmark_stop(uint64_t *total, uint64_t* number_schedules, uint64_t *kernel, uint64_t *entries)
{
    seL4_BenchmarkFinalizeLog();
    seL4_BenchmarkGetThreadUtilisation(TCB_CAP);
    uint64_t *buffer = (uint64_t *)&seL4_GetIPCBuffer()->msg[0];

    *total = buffer[BENCHMARK_TOTAL_UTILISATION];
    *number_schedules = buffer[BENCHMARK_TOTAL_NUMBER_SCHEDULES];
    *kernel = buffer[BENCHMARK_TOTAL_KERNEL_UTILISATION];
    *entries = buffer[BENCHMARK_TOTAL_NUMBER_KERNEL_ENTRIES];
}

static void microkit_benchmark_stop_tcb(uint64_t pd_id, uint64_t *total, uint64_t *number_schedules, uint64_t *kernel, uint64_t *entries)
{
    seL4_BenchmarkGetThreadUtilisation(BASE_TCB_CAP + pd_id);
    uint64_t *buffer = (uint64_t *)&seL4_GetIPCBuffer()->msg[0];

    *total = buffer[BENCHMARK_TCB_UTILISATION];
    *number_schedules = buffer[BENCHMARK_TCB_NUMBER_SCHEDULES];
    *kernel = buffer[BENCHMARK_TCB_KERNEL_UTILISATION];
    *entries = buffer[BENCHMARK_TCB_NUMBER_KERNEL_ENTRIES];
}

static void microkit_benchmark_stop_all(uint64_t core_clients)
{
    uint64_t total;
    uint64_t kernel;
    uint64_t entries;
    uint64_t number_schedules;
    microkit_benchmark_stop(&total, &number_schedules, &kernel, &entries);
    print_benchmark_details(PD_TOTAL, kernel, entries, number_schedules, total);
    
    for (uint64_t pd_id = 1; pd_id < 64; pd_id++) {
        if ((core_clients >> pd_id) & 1) {
            microkit_benchmark_stop_tcb(pd_id, &total, &number_schedules, &kernel, &entries);
            print_benchmark_details(pd_id, kernel, entries, number_schedules, total);
        }
    }
}

#endif