/*
 * SX1262 SPI clock sweep for proves-core-reference#602.
 *
 * For each requested SCK: WriteBuffer a 252 B pseudo-random pattern, ReadBuffer
 * it back, compare, and time the ReadBuffer spi_transceive() with the DWT cycle
 * counter. A lowest-priority thread counts while it has the CPU, so the CPU the
 * main thread actually consumed inside the call can be separated from wall time
 * in interrupt and DMA modes. Results go to the console as CSV lines.
 */

#include <stdlib.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/timing/timing.h>

#define LORA_NODE DT_NODELABEL(lora0)
#define SPI_NODE DT_BUS(LORA_NODE)
#define PCLK_HZ DT_PROP(DT_NODELABEL(clk_peri), clock_frequency)

#define SX_WRITE_BUFFER 0x0E
#define SX_READ_BUFFER 0x1E
#define SX_READ_REGISTER 0x1D
#define SX_SET_STANDBY 0x80
#define SX_GET_STATUS 0xC0

#define PAYLOAD 252
#define N_ITER 1000

#if defined(CONFIG_SPI_PL022_DMA)
#define MODE "dma"
#elif defined(CONFIG_SPI_PL022_INTERRUPT)
#define MODE "irq"
#else
#define MODE "poll"
#endif

static const struct spi_dt_spec lora_spec =
    SPI_DT_SPEC_GET(LORA_NODE, SPI_WORD_SET(8) | SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB);
static const struct gpio_dt_spec reset_gpio = GPIO_DT_SPEC_GET(LORA_NODE, reset_gpios);
static const struct gpio_dt_spec busy_gpio = GPIO_DT_SPEC_GET(LORA_NODE, busy_gpios);

/* 18.75 MHz is above the SX1262's 16 MHz limit; it is a signal-margin probe only. */
static const uint32_t req_hz[] = {125000, 1000000, 4000000, 8000000, 12000000, 16000000, 18750000};
/* spi_pl022 compares spi_config by pointer, so each clock gets its own struct. */
static struct spi_config cfgs[ARRAY_SIZE(req_hz)];
static struct spi_config cfg_write_ref;

static volatile uint32_t idle_ticks;
static void idle_counter(void* a, void* b, void* c) {
    for (;;) {
        idle_ticks++;
    }
}
K_THREAD_DEFINE(idle_thr, 512, idle_counter, NULL, NULL, NULL, K_LOWEST_APPLICATION_THREAD_PRIO, 0, 0);

static uint32_t ticks_per_ms;

static uint32_t wait_busy_us(void) {
    timing_t t0 = timing_counter_get();
    while (gpio_pin_get_dt(&busy_gpio)) {
        if (timing_cycles_to_ns(timing_cycles_get(&t0, &(timing_t){timing_counter_get()})) > 100000000ULL) {
            return UINT32_MAX;
        }
    }
    timing_t t1 = timing_counter_get();
    return (uint32_t)(timing_cycles_to_ns(timing_cycles_get(&t0, &t1)) / 1000);
}

static int xfer(const struct spi_config* cfg, const uint8_t* tx, uint8_t* rx, size_t len) {
    struct spi_buf tb = {.buf = (void*)tx, .len = len};
    struct spi_buf rb = {.buf = rx, .len = len};
    struct spi_buf_set ts = {.buffers = &tb, .count = 1};
    struct spi_buf_set rs = {.buffers = &rb, .count = 1};
    return spi_transceive(lora_spec.bus, cfg, &ts, rx ? &rs : NULL);
}

static uint32_t xorshift(uint32_t* s) {
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return x;
}

static int cmp_u32(const void* a, const void* b) {
    uint32_t x = *(const uint32_t*)a, y = *(const uint32_t*)b;
    return (x > y) - (x < y);
}

static uint32_t actual_hz(void) {
    volatile uint32_t* ssp = (volatile uint32_t*)DT_REG_ADDR(SPI_NODE);
    uint32_t scr = (ssp[0] >> 8) & 0xFF;
    uint32_t cpsr = ssp[4] & 0xFF;
    return PCLK_HZ / (cpsr * (scr + 1));
}

static uint32_t wall_us[N_ITER], cpu_us[N_ITER], busy_us[N_ITER];
static uint8_t tx[PAYLOAD + 3], rx[PAYLOAD + 3], pattern[PAYLOAD];

static void run_point(const struct spi_config* wr, const struct spi_config* rd, const char* pass) {
    uint32_t prng = 0x9E3779B9u ^ rd->frequency ^ (pass[0] << 24);
    uint32_t bad_xfers = 0, bad_bytes = 0, errs = 0, busy_timeouts = 0;
    uint32_t act = 0;

    for (int i = 0; i < N_ITER; i++) {
        for (int j = 0; j < PAYLOAD; j++) {
            pattern[j] = (uint8_t)xorshift(&prng);
        }
        tx[0] = SX_WRITE_BUFFER;
        tx[1] = 0;
        memcpy(&tx[2], pattern, PAYLOAD);
        if (wait_busy_us() == UINT32_MAX) {
            busy_timeouts++;
        }
        if (xfer(wr, tx, NULL, PAYLOAD + 2)) {
            errs++;
        }

        busy_us[i] = wait_busy_us();
        if (busy_us[i] == UINT32_MAX) {
            busy_timeouts++;
            busy_us[i] = 0;
        }
        memset(tx, 0, sizeof(tx));
        tx[0] = SX_READ_BUFFER;
        tx[1] = 0;
        memset(rx, 0xA5, sizeof(rx));

        uint32_t ticks0 = idle_ticks;
        timing_t t0 = timing_counter_get();
        int ret = xfer(rd, tx, rx, PAYLOAD + 3);
        timing_t t1 = timing_counter_get();
        uint32_t ticks1 = idle_ticks;

        if (ret) {
            errs++;
        }
        if (i == 0) {
            act = actual_hz();
        }
        wall_us[i] = (uint32_t)(timing_cycles_to_ns(timing_cycles_get(&t0, &t1)) / 1000);
        uint32_t free_us = (uint32_t)(((uint64_t)(ticks1 - ticks0) * 1000) / ticks_per_ms);
        cpu_us[i] = free_us >= wall_us[i] ? 0 : wall_us[i] - free_us;

        uint32_t nb = 0;
        for (int j = 0; j < PAYLOAD; j++) {
            nb += rx[3 + j] != pattern[j];
        }
        if (nb) {
            bad_xfers++;
            bad_bytes += nb;
            if (bad_xfers <= 3) {
                printk(
                    "MISMATCH %s rd=%u iter=%d bytes=%u first=[%02x %02x %02x %02x] "
                    "want=[%02x %02x %02x %02x]\n",
                    pass, rd->frequency, i, nb, rx[3], rx[4], rx[5], rx[6], pattern[0], pattern[1], pattern[2],
                    pattern[3]);
            }
        }
    }

    qsort(wall_us, N_ITER, sizeof(uint32_t), cmp_u32);
    qsort(cpu_us, N_ITER, sizeof(uint32_t), cmp_u32);
    qsort(busy_us, N_ITER, sizeof(uint32_t), cmp_u32);
    printk("CSV,%s,%s,%u,%u,%u,%d,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n", MODE, pass, wr->frequency, rd->frequency, act,
           N_ITER, wall_us[0], wall_us[N_ITER / 2], wall_us[N_ITER * 99 / 100], wall_us[N_ITER - 1], cpu_us[N_ITER / 2],
           cpu_us[N_ITER * 99 / 100], busy_us[N_ITER / 2], busy_us[N_ITER - 1], bad_xfers, bad_bytes, errs,
           busy_timeouts);
}

int main(void) {
    const struct device* con = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
    uint32_t dtr = 0;

    for (int i = 0; i < 600 && !dtr; i++) {
        uart_line_ctrl_get(con, UART_LINE_CTRL_DTR, &dtr);
        k_sleep(K_MSEC(100));
    }
    k_sleep(K_MSEC(500));

    timing_init();
    timing_start();

    printk("\nBENCH,bench-spi-sweep,mode=%s,pclk=%u,n=%d,payload=%d\n", MODE, PCLK_HZ, N_ITER, PAYLOAD);

    uint32_t ticks0 = idle_ticks;
    timing_t t0 = timing_counter_get();
    k_sleep(K_MSEC(500));
    timing_t t1 = timing_counter_get();
    uint32_t ticks1 = idle_ticks;
    uint64_t cal_ns = timing_cycles_to_ns(timing_cycles_get(&t0, &t1));
    ticks_per_ms = (uint32_t)(((uint64_t)(ticks1 - ticks0) * 1000000ULL) / cal_ns);
    printk("CAL,idle_ticks_per_ms=%u,cal_ns=%llu,timing_hz=%llu\n", ticks_per_ms, cal_ns, timing_freq_get());
    if (ticks_per_ms == 0) {
        printk("FATAL idle counter never ran\n");
        return 1;
    }

    if (!spi_is_ready_dt(&lora_spec) || !gpio_is_ready_dt(&reset_gpio) || !gpio_is_ready_dt(&busy_gpio)) {
        printk("FATAL devices not ready\n");
        return 1;
    }
    for (size_t i = 0; i < ARRAY_SIZE(req_hz); i++) {
        cfgs[i] = lora_spec.config;
        cfgs[i].frequency = req_hz[i];
    }
    cfg_write_ref = lora_spec.config;
    cfg_write_ref.frequency = 1000000;

    gpio_pin_configure_dt(&busy_gpio, GPIO_INPUT);
    gpio_pin_configure_dt(&reset_gpio, GPIO_OUTPUT_ACTIVE);
    k_busy_wait(500);
    gpio_pin_set_dt(&reset_gpio, 0);
    k_sleep(K_MSEC(5));
    uint32_t rst_busy = wait_busy_us();

    uint8_t cmd[6] = {SX_SET_STANDBY, 0x00};
    int ret = xfer(&cfg_write_ref, cmd, NULL, 2);
    wait_busy_us();

    uint8_t st[2] = {SX_GET_STATUS, 0x00}, str[2] = {0};
    ret |= xfer(&cfg_write_ref, st, str, 2);
    wait_busy_us();

    /* LoRa sync word registers 0x0740/0x0741 reset to 0x14 0x24 on SX126x. */
    uint8_t rr[6] = {SX_READ_REGISTER, 0x07, 0x40, 0x00, 0x00, 0x00}, rrr[6] = {0};
    ret |= xfer(&cfg_write_ref, rr, rrr, 6);
    wait_busy_us();
    printk(
        "IDENT,reset_busy_us=%u,status=0x%02x,chipmode=%u,cmdstatus=%u,reg0740=0x%02x,reg0741=0x%02x,"
        "spi_ret=%d\n",
        rst_busy, str[1], (str[1] >> 4) & 7, (str[1] >> 1) & 7, rrr[4], rrr[5], ret);

    printk(
        "CSVHDR,mode,pass,wr_hz,rd_hz,actual_hz,n,rd_min_us,rd_med_us,rd_p99_us,rd_max_us,"
        "cpu_med_us,cpu_p99_us,busy_med_us,busy_max_us,bad_xfers,bad_bytes,spi_errs,"
        "busy_timeouts\n");
    for (size_t i = 0; i < ARRAY_SIZE(req_hz); i++) {
        run_point(&cfgs[i], &cfgs[i], "same");
    }
    for (size_t i = 0; i < ARRAY_SIZE(req_hz); i++) {
        run_point(&cfg_write_ref, &cfgs[i], "wr1m");
    }
    printk("DONE,%s\n", MODE);
    for (;;) {
        k_sleep(K_SECONDS(10));
    }
    return 0;
}
