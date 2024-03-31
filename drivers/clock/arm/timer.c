#include <microkit.h>

#ifndef CONFIG_EXPORT_PCNT_USER
#error "ARM generic timer is not exported by seL4"
#endif

#define IRQ_CH 0

#define GENERIC_TIMER_ENABLE BIT(0)
#define GENERIC_TIMER_IMASK  BIT(1)
#define GENERIC_TIMER_STATUS BIT(2)

typedef struct {
    uint32_t freq;
} generic_timer_t;

static inline timer_properties_t get_generic_timer_properties(void)
{
    return (timer_properties_t) {
        .upcounter = true,
        .bit_width = 64
    };
}

static inline uint64_t generic_timer_get_ticks(void)
{
    uint64_t time;
    COPROC_READ_64(CNTPCT, time);
    return time;
}

static inline void generic_timer_set_compare(uint64_t ticks)
{
    COPROC_WRITE_64(CNTP_CVAL, ticks);
}

static inline uint32_t generic_timer_get_freq(void)
{
    uintptr_t freq;
    COPROC_READ_WORD(CNTFRQ, freq);
    return (uint32_t) freq;
}

static inline uint32_t generic_timer_read_ctrl(void)
{
    uintptr_t ctrl;
    COPROC_READ_WORD(CNTP_CTL, ctrl);
    return ctrl;
}

static inline void generic_timer_write_ctrl(uintptr_t ctrl)
{
    COPROC_WRITE_WORD(CNTP_CTL, ctrl);
}

static inline void generic_timer_or_ctrl(uintptr_t bits)
{
    uintptr_t ctrl = generic_timer_read_ctrl();
    generic_timer_write_ctrl(ctrl | bits);
}

static inline void generic_timer_and_ctrl(uintptr_t bits)
{
    uintptr_t ctrl = generic_timer_read_ctrl();
    generic_timer_write_ctrl(ctrl & bits);
}

static inline void generic_timer_enable(void)
{
    generic_timer_or_ctrl(GENERIC_TIMER_ENABLE);
}

static inline void generic_timer_disable(void)
{
    generic_timer_and_ctrl(~GENERIC_TIMER_ENABLE);
}

static inline void generic_timer_unmask_irq(void)
{
    generic_timer_and_ctrl(~GENERIC_TIMER_IMASK);
}

static inline void generic_timer_mask_irq(void)
{
    generic_timer_or_ctrl(GENERIC_TIMER_IMASK);
}

static inline uintptr_t generic_timer_status(void)
{
    return generic_timer_read_ctrl() & GENERIC_TIMER_STATUS;
}

// static int get_time(void *data, uint64_t *time)
// {
//     assert(data != NULL);
//     assert(time != NULL);

//     generic_ltimer_t *ltimer = data;
//     uint64_t ticks = generic_timer_get_ticks();
//     *time = freq_cycles_and_hz_to_ns(ticks, ltimer->freq);
//     return 0;
// }

void init() {
    generic_timer_set_compare(UINT64_MAX);
    generic_timer_enable();

    for (int i = 0; i < 100; i++) {
        printf("TICKS: 0x%lx\n", generic_timer_get_ticks());
    }
}

void notified(microkit_channel ch) {
    assert(ch == IRQ_CH)
}

