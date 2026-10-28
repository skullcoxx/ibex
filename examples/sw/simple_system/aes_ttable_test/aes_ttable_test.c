/*
 * Baseline: software-only T-table AES-128 for the Ibex simple system.
 * Measures retired instructions and cycles for:
 *   Enc-KeyExp, Dec-KeyExp, Enc (1 block), Dec (1 block)
 * Same four items as Tables 3/4 of Marshall et al. (TCHES 2021).
 *
 * Tables are generated at start-up (not measured) and live in RAM.
 * Words are column-packed little-endian (row 0 = byte 0), same as the V3 model.
 *
 * Target build:  make          (inside this directory, with env.sh sourced)
 * Host check:    gcc -O2 -DHOST -o /tmp/ttable aes_ttable_test.c && /tmp/ttable
 */
#include <stdint.h>

#ifdef HOST
#include <stdio.h>
#define OUT(s) fputs((s), stdout)
static inline uint32_t rd_cycle(void)  { return 0; }
static inline uint32_t rd_instret(void) { return 0; }
#else
#include "simple_system_common.h"
#define OUT(s) puts(s)   /* ibex puts() does not append a newline */
static inline uint32_t rd_cycle(void) {
    uint32_t v; asm volatile("csrr %0, mcycle" : "=r"(v)); return v;
}
static inline uint32_t rd_instret(void) {
    uint32_t v; asm volatile("csrr %0, minstret" : "=r"(v)); return v;
}
#endif

/* ---------------- tables ---------------- */
static uint8_t  sbox[256], isbox[256];
static uint32_t Te0[256], Te1[256], Te2[256], Te3[256];
static uint32_t Td0[256], Td1[256], Td2[256], Td3[256];
static uint32_t rk[44], dk[44];

static uint32_t rotl32(uint32_t x, unsigned n) { return (x << n) | (x >> (32 - n)); }
static uint32_t rotr32(uint32_t x, unsigned n) { return (x >> n) | (x << (32 - n)); }
static uint8_t  rotl8(uint8_t x, unsigned n)   { return (uint8_t)((x << n) | (x >> (8 - n))); }

static uint8_t gmul(uint8_t a, uint8_t b) {
    uint8_t p = 0;
    while (b) {
        if (b & 1) p ^= a;
        a = (uint8_t)((a << 1) ^ ((a & 0x80) ? 0x1b : 0));
        b >>= 1;
    }
    return p;
}

static void init_tables(void) {
    uint8_t p = 1, q = 1;
    do {
        p = (uint8_t)(p ^ (p << 1) ^ ((p & 0x80) ? 0x1b : 0));
        q ^= (uint8_t)(q << 1); q ^= (uint8_t)(q << 2); q ^= (uint8_t)(q << 4);
        if (q & 0x80) q ^= 0x09;
        sbox[p] = (uint8_t)(q ^ rotl8(q, 1) ^ rotl8(q, 2) ^ rotl8(q, 3) ^ rotl8(q, 4) ^ 0x63);
    } while (p != 1);
    sbox[0] = 0x63;
    for (int i = 0; i < 256; i++) isbox[sbox[i]] = (uint8_t)i;

    for (int i = 0; i < 256; i++) {
        uint8_t s = sbox[i];
        Te0[i] = (uint32_t)gmul(s, 2) | ((uint32_t)s << 8) | ((uint32_t)s << 16) | ((uint32_t)gmul(s, 3) << 24);
        Te1[i] = rotl32(Te0[i], 8); Te2[i] = rotl32(Te0[i], 16); Te3[i] = rotl32(Te0[i], 24);
        uint8_t v = isbox[i];
        Td0[i] = (uint32_t)gmul(v, 14) | ((uint32_t)gmul(v, 9) << 8) | ((uint32_t)gmul(v, 13) << 16) | ((uint32_t)gmul(v, 11) << 24);
        Td1[i] = rotl32(Td0[i], 8); Td2[i] = rotl32(Td0[i], 16); Td3[i] = rotl32(Td0[i], 24);
    }
}

static uint32_t ld32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static void st32(uint8_t *p, uint32_t w) {
    p[0] = (uint8_t)w; p[1] = (uint8_t)(w >> 8); p[2] = (uint8_t)(w >> 16); p[3] = (uint8_t)(w >> 24);
}

/* ---------------- AES-128 (T-table) ---------------- */
static uint32_t sub_word(uint32_t w) {
    return (uint32_t)sbox[w & 0xff] | ((uint32_t)sbox[(w >> 8) & 0xff] << 8) |
           ((uint32_t)sbox[(w >> 16) & 0xff] << 16) | ((uint32_t)sbox[w >> 24] << 24);
}

static void __attribute__((noinline)) key_expand_enc(const uint8_t key[16]) {
    static const uint8_t rcon[10] = {0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80,0x1b,0x36};
    for (int i = 0; i < 4; i++) rk[i] = ld32(key + 4 * i);
    for (int i = 4; i < 44; i++) {
        uint32_t t = rk[i - 1];
        if ((i & 3) == 0) t = sub_word(rotr32(t, 8)) ^ rcon[(i >> 2) - 1];
        rk[i] = rk[i - 4] ^ t;
    }
}

static uint32_t inv_mix_word(uint32_t w) {
    return Td0[sbox[w & 0xff]] ^ Td1[sbox[(w >> 8) & 0xff]] ^
           Td2[sbox[(w >> 16) & 0xff]] ^ Td3[sbox[w >> 24]];
}

/* Equivalent inverse cipher key schedule; requires key_expand_enc() first. */
static void __attribute__((noinline)) key_expand_dec(void) {
    for (int r = 0; r <= 10; r++)
        for (int c = 0; c < 4; c++) {
            uint32_t w = rk[4 * (10 - r) + c];
            dk[4 * r + c] = (r == 0 || r == 10) ? w : inv_mix_word(w);
        }
}

static void __attribute__((noinline)) aes_enc(const uint8_t in[16], uint8_t out[16]) {
    uint32_t s0 = ld32(in) ^ rk[0], s1 = ld32(in + 4) ^ rk[1];
    uint32_t s2 = ld32(in + 8) ^ rk[2], s3 = ld32(in + 12) ^ rk[3];
    const uint32_t *k = rk + 4;
    for (int r = 1; r < 10; r++, k += 4) {
        uint32_t t0 = Te0[s0 & 0xff] ^ Te1[(s1 >> 8) & 0xff] ^ Te2[(s2 >> 16) & 0xff] ^ Te3[s3 >> 24] ^ k[0];
        uint32_t t1 = Te0[s1 & 0xff] ^ Te1[(s2 >> 8) & 0xff] ^ Te2[(s3 >> 16) & 0xff] ^ Te3[s0 >> 24] ^ k[1];
        uint32_t t2 = Te0[s2 & 0xff] ^ Te1[(s3 >> 8) & 0xff] ^ Te2[(s0 >> 16) & 0xff] ^ Te3[s1 >> 24] ^ k[2];
        uint32_t t3 = Te0[s3 & 0xff] ^ Te1[(s0 >> 8) & 0xff] ^ Te2[(s1 >> 16) & 0xff] ^ Te3[s2 >> 24] ^ k[3];
        s0 = t0; s1 = t1; s2 = t2; s3 = t3;
    }
    uint32_t t0 = (uint32_t)sbox[s0 & 0xff] | ((uint32_t)sbox[(s1 >> 8) & 0xff] << 8) | ((uint32_t)sbox[(s2 >> 16) & 0xff] << 16) | ((uint32_t)sbox[s3 >> 24] << 24);
    uint32_t t1 = (uint32_t)sbox[s1 & 0xff] | ((uint32_t)sbox[(s2 >> 8) & 0xff] << 8) | ((uint32_t)sbox[(s3 >> 16) & 0xff] << 16) | ((uint32_t)sbox[s0 >> 24] << 24);
    uint32_t t2 = (uint32_t)sbox[s2 & 0xff] | ((uint32_t)sbox[(s3 >> 8) & 0xff] << 8) | ((uint32_t)sbox[(s0 >> 16) & 0xff] << 16) | ((uint32_t)sbox[s1 >> 24] << 24);
    uint32_t t3 = (uint32_t)sbox[s3 & 0xff] | ((uint32_t)sbox[(s0 >> 8) & 0xff] << 8) | ((uint32_t)sbox[(s1 >> 16) & 0xff] << 16) | ((uint32_t)sbox[s2 >> 24] << 24);
    st32(out, t0 ^ k[0]); st32(out + 4, t1 ^ k[1]); st32(out + 8, t2 ^ k[2]); st32(out + 12, t3 ^ k[3]);
}

static void __attribute__((noinline)) aes_dec(const uint8_t in[16], uint8_t out[16]) {
    uint32_t s0 = ld32(in) ^ dk[0], s1 = ld32(in + 4) ^ dk[1];
    uint32_t s2 = ld32(in + 8) ^ dk[2], s3 = ld32(in + 12) ^ dk[3];
    const uint32_t *k = dk + 4;
    for (int r = 1; r < 10; r++, k += 4) {
        uint32_t t0 = Td0[s0 & 0xff] ^ Td1[(s3 >> 8) & 0xff] ^ Td2[(s2 >> 16) & 0xff] ^ Td3[s1 >> 24] ^ k[0];
        uint32_t t1 = Td0[s1 & 0xff] ^ Td1[(s0 >> 8) & 0xff] ^ Td2[(s3 >> 16) & 0xff] ^ Td3[s2 >> 24] ^ k[1];
        uint32_t t2 = Td0[s2 & 0xff] ^ Td1[(s1 >> 8) & 0xff] ^ Td2[(s0 >> 16) & 0xff] ^ Td3[s3 >> 24] ^ k[2];
        uint32_t t3 = Td0[s3 & 0xff] ^ Td1[(s2 >> 8) & 0xff] ^ Td2[(s1 >> 16) & 0xff] ^ Td3[s0 >> 24] ^ k[3];
        s0 = t0; s1 = t1; s2 = t2; s3 = t3;
    }
    uint32_t t0 = (uint32_t)isbox[s0 & 0xff] | ((uint32_t)isbox[(s3 >> 8) & 0xff] << 8) | ((uint32_t)isbox[(s2 >> 16) & 0xff] << 16) | ((uint32_t)isbox[s1 >> 24] << 24);
    uint32_t t1 = (uint32_t)isbox[s1 & 0xff] | ((uint32_t)isbox[(s0 >> 8) & 0xff] << 8) | ((uint32_t)isbox[(s3 >> 16) & 0xff] << 16) | ((uint32_t)isbox[s2 >> 24] << 24);
    uint32_t t2 = (uint32_t)isbox[s2 & 0xff] | ((uint32_t)isbox[(s1 >> 8) & 0xff] << 8) | ((uint32_t)isbox[(s0 >> 16) & 0xff] << 16) | ((uint32_t)isbox[s3 >> 24] << 24);
    uint32_t t3 = (uint32_t)isbox[s3 & 0xff] | ((uint32_t)isbox[(s2 >> 8) & 0xff] << 8) | ((uint32_t)isbox[(s1 >> 16) & 0xff] << 16) | ((uint32_t)isbox[s0 >> 24] << 24);
    st32(out, t0 ^ k[0]); st32(out + 4, t1 ^ k[1]); st32(out + 8, t2 ^ k[2]); st32(out + 12, t3 ^ k[3]);
}

/* ---------------- measurement helpers ---------------- */
static void __attribute__((noinline)) empty_call(void) { asm volatile(""); }

static void print_dec(uint32_t v) {
    char buf[11]; int n = 0;
    if (v == 0) buf[n++] = '0';
    while (v) { buf[n++] = (char)('0' + v % 10); v /= 10; }
    char s[12]; int m = 0;
    while (n) s[m++] = buf[--n];
    s[m] = 0;
    OUT(s);
}

static void report(const char *name, uint32_t iret, uint32_t cyc, uint32_t iret0, uint32_t cyc0) {
    OUT(name); OUT("  iret="); print_dec(iret - iret0);
    OUT("  cycles="); print_dec(cyc - cyc0); OUT("\n");
}

#define MEASURE(NAME, CALL)                                                   \
    do {                                                                      \
        uint32_t c0_ = rd_cycle(), i0_ = rd_instret();                        \
        CALL;                                                                 \
        uint32_t i1_ = rd_instret(), c1_ = rd_cycle();                        \
        report(NAME, i1_ - i0_, c1_ - c0_, ov_i, ov_c);                       \
    } while (0)

/* ---------------- main ---------------- */
static int same(const uint8_t *a, const uint8_t *b) {
    for (int i = 0; i < 16; i++) if (a[i] != b[i]) return 0;
    return 1;
}

int main(void) {
    static const uint8_t key[16] = {0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f};
    static const uint8_t pt [16] = {0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff};
    static const uint8_t ct [16] = {0x69,0xc4,0xe0,0xd8,0x6a,0x7b,0x04,0x30,0xd8,0xcd,0xb7,0x80,0x70,0xb4,0xc5,0x5a};
    uint8_t out[16];

#ifndef HOST
    pcount_reset();
    pcount_enable(1);
#endif
    init_tables();

    /* Calibrate: an empty call measured with the same read sequence gives the
     * cost of the counter reads plus one call/return; it is subtracted below. */
    uint32_t c0 = rd_cycle(), i0 = rd_instret();
    empty_call();
    uint32_t i1 = rd_instret(), c1 = rd_cycle();
    uint32_t ov_i = i1 - i0, ov_c = c1 - c0;

    OUT("T-table AES-128 baseline\n");

    MEASURE("Enc-KeyExp", key_expand_enc(key));
    MEASURE("Dec-KeyExp", key_expand_dec());
    MEASURE("Enc       ", aes_enc(pt, out));
    int enc_ok = same(out, ct);
    MEASURE("Dec       ", aes_dec(ct, out));
    int dec_ok = same(out, pt);

    OUT(enc_ok ? "encrypt: PASS\n" : "encrypt: FAIL\n");
    OUT(dec_ok ? "decrypt: PASS\n" : "decrypt: FAIL\n");
    OUT("(counter-read and call/return overhead subtracted via an empty call)\n");

#ifndef HOST
    pcount_enable(0);
#endif
    return (enc_ok && dec_ok) ? 0 : 1;
}
