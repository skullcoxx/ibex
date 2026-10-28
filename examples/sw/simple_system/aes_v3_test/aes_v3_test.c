/*
 * AES-128 using the V3 scalar AES ISE (custom-0 opcode 0x0b), for the Ibex simple system.
 * Same measurements as aes_ttable_test.c: Enc-KeyExp, Dec-KeyExp, Enc (1 block), Dec (1 block).
 * No lookup tables are needed: SubWord, InvMixColumns of the key schedule and all rounds are
 * built from the four instructions (encs, encsm, decs, decsm) and XOR.
 *
 *   instruction : funct3 = 0 encs, 1 encsm, 2 decs, 3 decsm ; funct7 = bs << 5
 *
 * Target build:  make          (inside this directory, with env.sh sourced)
 * Host check:    gcc -O2 -DHOST -I<dir with aes_v3_model.c> -o /tmp/v3test aes_v3_test.c && /tmp/v3test
 */
#include <stdint.h>

#define ENCS  0
#define ENCSM 1
#define DECS  2
#define DECSM 3

#ifdef HOST
#include <stdio.h>
/* Reuse only the golden model's instruction function; rename its helpers to avoid clashes. */
#define main aes_model_main
#define sub_word model_sub_word
#define inv_mix_word model_inv_mix_word
#define key_expand_enc model_key_expand_enc
#define key_expand_dec model_key_expand_dec
#include "aes_v3_model.c"
#undef main
#undef sub_word
#undef inv_mix_word
#undef key_expand_enc
#undef key_expand_dec
#define OUT(s) fputs((s), stdout)
static inline uint32_t rd_cycle(void)   { return 0; }
static inline uint32_t rd_instret(void) { return 0; }
/* Host stand-in: the C golden model of the instruction. */
#define SAES(OP, BS, A, W) saes_v3_proc((A), (W), (BS), ((OP) < 2), (OP) & 1)
#else
#include "simple_system_common.h"
#define OUT(s) puts(s)   /* ibex puts() does not append a newline */
static inline uint32_t rd_cycle(void) {
    uint32_t v; asm volatile("csrr %0, mcycle" : "=r"(v)); return v;
}
static inline uint32_t rd_instret(void) {
    uint32_t v; asm volatile("csrr %0, minstret" : "=r"(v)); return v;
}
#define STR_(x) #x
#define XSTR(x) STR_(x)
#define F7_0 0x00
#define F7_1 0x20
#define F7_2 0x40
#define F7_3 0x60
#define SAES_ASM(F3, F7, A, W)                                                   \
  ({ uint32_t r_;                                                                \
     asm volatile(".insn r 0x0b, " XSTR(F3) ", " XSTR(F7) ", %0, %1, %2"         \
                  : "=r"(r_) : "r"((uint32_t)(A)), "r"((uint32_t)(W)));          \
     r_; })
/* BS must be a literal 0..3 */
#define SAES(OP, BS, A, W) SAES_ASM(OP, F7_##BS, A, W)
#endif

static uint32_t rk[44], dk[44];

static uint32_t rotr32(uint32_t x, unsigned n) { return (x >> n) | (x << (32 - n)); }

/* Word access. The state buffers are 4-byte aligned (see main), so these compile to lw/sw. */
static inline __attribute__((always_inline)) uint32_t ld32(const uint8_t *p) {
    uint32_t v;
    __builtin_memcpy(&v, __builtin_assume_aligned(p, 4), 4);
    return v;
}
static inline __attribute__((always_inline)) void st32(uint8_t *p, uint32_t w) {
    __builtin_memcpy(__builtin_assume_aligned(p, 4), &w, 4);
}

/* SubWord: 4 x encs (accumulates S(w.byte[i]) at byte position i). */
static uint32_t sub_word(uint32_t w) {
    uint32_t r = 0;
    r = SAES(ENCS, 0, r, w);
    r = SAES(ENCS, 1, r, w);
    r = SAES(ENCS, 2, r, w);
    r = SAES(ENCS, 3, r, w);
    return r;
}

/* InvMixColumns of one word: 4 x encs (S-box) then 4 x decsm (inverse S-box + InvMix) = 8 instr. */
static uint32_t inv_mix_word(uint32_t w) {
    uint32_t tmp = sub_word(w), r = 0;
    r = SAES(DECSM, 0, r, tmp);
    r = SAES(DECSM, 1, r, tmp);
    r = SAES(DECSM, 2, r, tmp);
    r = SAES(DECSM, 3, r, tmp);
    return r;
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
        uint32_t t0 = k[0], t1 = k[1], t2 = k[2], t3 = k[3];
        t0 = SAES(ENCSM, 0, t0, s0); t0 = SAES(ENCSM, 1, t0, s1); t0 = SAES(ENCSM, 2, t0, s2); t0 = SAES(ENCSM, 3, t0, s3);
        t1 = SAES(ENCSM, 0, t1, s1); t1 = SAES(ENCSM, 1, t1, s2); t1 = SAES(ENCSM, 2, t1, s3); t1 = SAES(ENCSM, 3, t1, s0);
        t2 = SAES(ENCSM, 0, t2, s2); t2 = SAES(ENCSM, 1, t2, s3); t2 = SAES(ENCSM, 2, t2, s0); t2 = SAES(ENCSM, 3, t2, s1);
        t3 = SAES(ENCSM, 0, t3, s3); t3 = SAES(ENCSM, 1, t3, s0); t3 = SAES(ENCSM, 2, t3, s1); t3 = SAES(ENCSM, 3, t3, s2);
        s0 = t0; s1 = t1; s2 = t2; s3 = t3;
    }
    uint32_t t0 = k[0], t1 = k[1], t2 = k[2], t3 = k[3];   /* final round: no MixColumns */
    t0 = SAES(ENCS, 0, t0, s0); t0 = SAES(ENCS, 1, t0, s1); t0 = SAES(ENCS, 2, t0, s2); t0 = SAES(ENCS, 3, t0, s3);
    t1 = SAES(ENCS, 0, t1, s1); t1 = SAES(ENCS, 1, t1, s2); t1 = SAES(ENCS, 2, t1, s3); t1 = SAES(ENCS, 3, t1, s0);
    t2 = SAES(ENCS, 0, t2, s2); t2 = SAES(ENCS, 1, t2, s3); t2 = SAES(ENCS, 2, t2, s0); t2 = SAES(ENCS, 3, t2, s1);
    t3 = SAES(ENCS, 0, t3, s3); t3 = SAES(ENCS, 1, t3, s0); t3 = SAES(ENCS, 2, t3, s1); t3 = SAES(ENCS, 3, t3, s2);
    st32(out, t0); st32(out + 4, t1); st32(out + 8, t2); st32(out + 12, t3);
}

static void __attribute__((noinline)) aes_dec(const uint8_t in[16], uint8_t out[16]) {
    uint32_t s0 = ld32(in) ^ dk[0], s1 = ld32(in + 4) ^ dk[1];
    uint32_t s2 = ld32(in + 8) ^ dk[2], s3 = ld32(in + 12) ^ dk[3];
    const uint32_t *k = dk + 4;
    for (int r = 1; r < 10; r++, k += 4) {
        uint32_t t0 = k[0], t1 = k[1], t2 = k[2], t3 = k[3];
        t0 = SAES(DECSM, 0, t0, s0); t0 = SAES(DECSM, 1, t0, s3); t0 = SAES(DECSM, 2, t0, s2); t0 = SAES(DECSM, 3, t0, s1);
        t1 = SAES(DECSM, 0, t1, s1); t1 = SAES(DECSM, 1, t1, s0); t1 = SAES(DECSM, 2, t1, s3); t1 = SAES(DECSM, 3, t1, s2);
        t2 = SAES(DECSM, 0, t2, s2); t2 = SAES(DECSM, 1, t2, s1); t2 = SAES(DECSM, 2, t2, s0); t2 = SAES(DECSM, 3, t2, s3);
        t3 = SAES(DECSM, 0, t3, s3); t3 = SAES(DECSM, 1, t3, s2); t3 = SAES(DECSM, 2, t3, s1); t3 = SAES(DECSM, 3, t3, s0);
        s0 = t0; s1 = t1; s2 = t2; s3 = t3;
    }
    uint32_t t0 = k[0], t1 = k[1], t2 = k[2], t3 = k[3];
    t0 = SAES(DECS, 0, t0, s0); t0 = SAES(DECS, 1, t0, s3); t0 = SAES(DECS, 2, t0, s2); t0 = SAES(DECS, 3, t0, s1);
    t1 = SAES(DECS, 0, t1, s1); t1 = SAES(DECS, 1, t1, s0); t1 = SAES(DECS, 2, t1, s3); t1 = SAES(DECS, 3, t1, s2);
    t2 = SAES(DECS, 0, t2, s2); t2 = SAES(DECS, 1, t2, s1); t2 = SAES(DECS, 2, t2, s0); t2 = SAES(DECS, 3, t2, s3);
    t3 = SAES(DECS, 0, t3, s3); t3 = SAES(DECS, 1, t3, s2); t3 = SAES(DECS, 2, t3, s1); t3 = SAES(DECS, 3, t3, s0);
    st32(out, t0); st32(out + 4, t1); st32(out + 8, t2); st32(out + 12, t3);
}

/* ---------------- measurement helpers (same as the T-table baseline) ---------------- */
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

static int same(const uint8_t *a, const uint8_t *b) {
    for (int i = 0; i < 16; i++) if (a[i] != b[i]) return 0;
    return 1;
}

int main(void) {
    static const uint8_t key[16] __attribute__((aligned(4))) = {0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f};
    static const uint8_t pt [16] __attribute__((aligned(4))) = {0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff};
    static const uint8_t ct [16] __attribute__((aligned(4))) = {0x69,0xc4,0xe0,0xd8,0x6a,0x7b,0x04,0x30,0xd8,0xcd,0xb7,0x80,0x70,0xb4,0xc5,0x5a};
    uint8_t out[16] __attribute__((aligned(4)));

#ifdef HOST
    gen_sboxes();
#else
    pcount_reset();
    pcount_enable(1);
#endif

    /* Calibrate: an empty call measured with the same read sequence gives the cost of the
     * counter reads plus one call/return; it is subtracted below. */
    uint32_t c0 = rd_cycle(), i0 = rd_instret();
    empty_call();
    uint32_t i1 = rd_instret(), c1 = rd_cycle();
    uint32_t ov_i = i1 - i0, ov_c = c1 - c0;

    OUT("V3 AES-128\n");

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
