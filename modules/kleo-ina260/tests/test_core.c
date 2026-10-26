/* SPDX-License-Identifier: Apache-2.0
 * Exercises the production C core against a fake I2C register transport.
 */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "ina260_core.h"

struct fake {
    uint16_t regs[256];
    int fail_reg, write_error, reads, writes, polls, ready_after;
    int64_t ms;
    bool mismatch;
};
static void reset(struct fake *f)
{
    memset(f, 0, sizeof(*f));
    f->fail_reg = -1;
    f->ready_after = 2;
    f->regs[0xfe] = 0x5449;
    f->regs[0xff] = 0x2273; /* Revision nibble is deliberately nonzero. */
    f->regs[1] = 0xfce0; /* -1 A */
    f->regs[2] = 4000;   /* 5 V */
    f->regs[3] = 500;    /* 5 W magnitude */
}
static int rd(void *ctx, uint8_t reg, uint8_t bytes[2])
{
    struct fake *f = ctx;
    ++f->reads;
    if (f->fail_reg == reg) { return -EIO; }
    assert(reg == 0 || reg == 1 || reg == 2 || reg == 3 ||
           reg == 6 || reg == 0xfe || reg == 0xff);
    uint16_t value = f->regs[reg];
    if (reg == 6 && f->polls++ >= f->ready_after) { value |= 8; }
    if (reg == 0 && f->mismatch) { value ^= 1; }
    bytes[0] = value >> 8;
    bytes[1] = value & 255;
    return 0;
}
static int wr(void *ctx, const uint8_t *bytes, size_t n)
{
    struct fake *f = ctx;
    ++f->writes;
    /* Only a correctly ordered triggered configuration is permitted. */
    assert(n == 3 && bytes[0] == 0 && bytes[1] == 0x61 && bytes[2] == 0x23);
    if (f->write_error) { return f->write_error; }
    f->regs[0] = 0x6123;
    f->polls = 0;
    return 0;
}
static int64_t now(void *ctx) { return ((struct fake *)ctx)->ms; }
static void sleep_ms(void *ctx, uint32_t ms) { ((struct fake *)ctx)->ms += ms; }
static struct ina260_io io_for(struct fake *f)
{
    return (struct ina260_io){f, rd, wr, now, sleep_ms};
}
int main(void)
{
    struct fake f;
    reset(&f);
    struct ina260_io io = io_for(&f);
    struct ina260_sample s = {0};

    assert(ina260_current_micro(0) == 0);
    assert(ina260_current_micro(1) == 1250);
    assert(ina260_current_micro(0xffff) == -1250);
    assert(ina260_current_micro(0xfce0) == -1000000);
    assert(ina260_current_micro(0x8000) == -40960000);
    assert(ina260_current_micro(0x7fff) == 40958750);
    assert(ina260_voltage_micro(4000) == 5000000);
    assert(ina260_voltage_micro(0x7fff) == 40958750);
    assert(ina260_power_micro(500) == 5000000);
    assert(ina260_power_micro(0x8000) == 327680000);
    assert(ina260_power_micro(0xffff) == 655350000);
    puts("PASS: signed current, voltage and unsigned power scaling/boundaries");

    assert(ina260_identify(&io) == 0 && f.writes == 0);
    f.regs[0xfe] = 0x4954;
    assert(ina260_fetch(&io, &s) == -ENODEV && !s.valid && f.writes == 0);
    reset(&f); f.regs[0xff] = 0x2260;
    assert(ina260_fetch(&io, &s) == -ENODEV && f.writes == 0);
    puts("PASS: ID endian handling, revision mask and wrong-device rejection");

    reset(&f);
    assert(ina260_fetch(&io, &s) == 0 && s.valid);
    assert(s.current_raw == 0xfce0 && s.voltage_raw == 4000 && s.power_raw == 500);
    assert(s.timestamp_ms == 2 && f.writes == 1);
    struct ina260_sample prior = s;
    const int failures[] = {0xfe, 0xff, 0, 6, 1, 2, 3};
    for (unsigned int n = 0; n < sizeof(failures)/sizeof(failures[0]); ++n) {
        reset(&f); s = prior; f.fail_reg = failures[n]; f.regs[1] = 123;
        assert(ina260_fetch(&io, &s) == -EIO && !s.valid);
        assert(s.current_raw == prior.current_raw && s.voltage_raw == prior.voltage_raw &&
               s.power_raw == prior.power_raw && s.timestamp_ms == prior.timestamp_ms);
    }
    reset(&f); f.write_error = -EIO;
    assert(ina260_fetch(&io, &s) == -EIO && !s.valid);
    reset(&f); f.mismatch = true;
    assert(ina260_fetch(&io, &s) == -EIO && !s.valid);
    puts("PASS: transport failures, config readback and atomic sample commit");

    reset(&f); f.ready_after = 1000;
    assert(ina260_fetch(&io, &s) == -ETIMEDOUT && !s.valid && f.ms == 20);
    reset(&f); f.regs[6] = 4;
    assert(ina260_fetch(&io, &s) == -ERANGE && !s.valid);
    reset(&f); f.regs[2] = 0x8000;
    assert(ina260_fetch(&io, &s) == -ERANGE && !s.valid);
    puts("PASS: bounded conversion polling, overflow and invalid voltage");

    /* Simulate sensor power cycle; the next fetch configures it afresh. */
    reset(&f);
    assert(ina260_fetch(&io, &s) == 0 && s.valid);
    reset(&f);
    assert(ina260_fetch(&io, &s) == 0 && s.valid && f.writes == 1);
    struct fake second; reset(&second); second.regs[1] = 800;
    struct ina260_io io2 = io_for(&second);
    struct ina260_sample s2 = {0};
    assert(ina260_fetch(&io2, &s2) == 0);
    assert(s.current_raw == 0xfce0 && s2.current_raw == 800);
    puts("PASS: recovery after failed sample/power cycle and instance isolation");
    puts("ALL CORE TESTS PASSED (simulated I2C, not hardware validation)");
    return 0;
}
