/*
 * Smoke test for the V3 scalar AES ISE on Ibex (custom-0 opcode 0x0b, R-type).
 *   funct3 : 0 encs, 1 encsm, 2 decs, 3 decsm
 *   funct7 : byte select bs in instr[31:30]  ->  funct7 = bs << 5
 * Expected values come from the C golden model (aes_v3_model.c).
 * If the simulator was NOT rebuilt with the AES patch, the first CHECK traps as an
 * illegal instruction. In that case stop the simulation with Ctrl-C.
 */
#include <stdint.h>
#include "simple_system_common.h"

static int fails;
static int total;

#define CHECK(F3, F7, A, B, EXP)                                              \
  do {                                                                        \
    uint32_t r_;                                                              \
    asm volatile(".insn r 0x0b, " #F3 ", " #F7 ", %0, %1, %2"                 \
                 : "=r"(r_) : "r"((uint32_t)(A)), "r"((uint32_t)(B)));        \
    total++;                                                                  \
    if (r_ != (uint32_t)(EXP)) {                                              \
      fails++;                                                                \
      puts("FAIL funct3=" #F3 " funct7=" #F7 " got=");                        \
      puthex(r_); puts(" exp="); puthex((uint32_t)(EXP)); puts("\n");        \
    }                                                                         \
  } while (0)

int main(void) {
  puts("V3 AES ISE smoke test\n");
  CHECK(0, 0x00, 0x9a87c16fu, 0x37e8177au, 0x9a87c1b5u); /* encs bs=0 */
  CHECK(0, 0x00, 0x95eef2d9u, 0x3d0b1763u, 0x95eef222u); /* encs bs=0 */
  CHECK(0, 0x00, 0xa30c4a90u, 0x6195a85fu, 0xa30c4a5fu); /* encs bs=0 */
  CHECK(0, 0x20, 0x475a6010u, 0x6054d23cu, 0x475ad510u); /* encs bs=1 */
  CHECK(0, 0x20, 0xb876c995u, 0x49cf7fd7u, 0xb8761b95u); /* encs bs=1 */
  CHECK(0, 0x20, 0x608c556du, 0xae118fddu, 0x608c266du); /* encs bs=1 */
  CHECK(0, 0x40, 0x62a66528u, 0x6e657709u, 0x62eb6528u); /* encs bs=2 */
  CHECK(0, 0x40, 0xd002de2bu, 0xf33882c8u, 0xd005de2bu); /* encs bs=2 */
  CHECK(0, 0x40, 0x8f5f9c78u, 0xe6c4e010u, 0x8f439c78u); /* encs bs=2 */
  CHECK(0, 0x60, 0x221d7313u, 0x0395f0acu, 0x591d7313u); /* encs bs=3 */
  CHECK(0, 0x60, 0x0d85e3ecu, 0x8eff5910u, 0x1485e3ecu); /* encs bs=3 */
  CHECK(0, 0x60, 0xde70143eu, 0x4200f2e5u, 0xf270143eu); /* encs bs=3 */
  CHECK(1, 0x00, 0xd7d3e5abu, 0xc7bcd318u, 0x3b7e48eau); /* encsm bs=0 */
  CHECK(1, 0x00, 0xe6204317u, 0x261852f6u, 0x20620193u); /* encsm bs=0 */
  CHECK(1, 0x00, 0xa4961e75u, 0x8c4936f9u, 0x140f875cu); /* encsm bs=0 */
  CHECK(1, 0x20, 0xf85e35f2u, 0xe2ed3ac2u, 0x78de2e69u); /* encsm bs=1 */
  CHECK(1, 0x20, 0xf31e5b18u, 0x37d5a3e6u, 0xf9144f06u); /* encsm bs=1 */
  CHECK(1, 0x20, 0xf68d6472u, 0x1a622713u, 0x3a41e73du); /* encsm bs=1 */
  CHECK(1, 0x40, 0x8e8d6233u, 0x7ba97996u, 0x5d300ce0u); /* encsm bs=2 */
  CHECK(1, 0x40, 0xc7e9e8bbu, 0xa5204d44u, 0x709c2a0cu); /* encsm bs=2 */
  CHECK(1, 0x40, 0x3d9bfb00u, 0x1d8095ddu, 0xf01ab7cdu); /* encsm bs=2 */
  CHECK(1, 0x60, 0xe85d7a40u, 0xa5be608au, 0xe4577c46u); /* encsm bs=3 */
  CHECK(1, 0x60, 0x5c4d9ffdu, 0x19fb4ca4u, 0xef2a4b29u); /* encsm bs=3 */
  CHECK(1, 0x60, 0x7d9166f3u, 0x78aae4d4u, 0x1e4eda4fu); /* encsm bs=3 */
  CHECK(2, 0x00, 0xa23eaf4cu, 0x8d358047u, 0xa23eaf5au); /* decs bs=0 */
  CHECK(2, 0x00, 0x9a92a5f9u, 0x0b91bf0fu, 0x9a92a502u); /* decs bs=0 */
  CHECK(2, 0x00, 0xb27867d7u, 0x6ed9ca56u, 0xb278676eu); /* decs bs=0 */
  CHECK(2, 0x20, 0xa5f7127fu, 0xb0a2e5c3u, 0xa5f7387fu); /* decs bs=1 */
  CHECK(2, 0x20, 0x6f448a0eu, 0x3ea3948cu, 0x6f446d0eu); /* decs bs=1 */
  CHECK(2, 0x20, 0xca746035u, 0x882e828cu, 0xca747135u); /* decs bs=1 */
  CHECK(2, 0x40, 0x579af8d3u, 0x18930ef3u, 0x57b8f8d3u); /* decs bs=2 */
  CHECK(2, 0x40, 0x50e718f5u, 0xccc5c029u, 0x50e018f5u); /* decs bs=2 */
  CHECK(2, 0x40, 0xecdb9369u, 0x48697a72u, 0xec3f9369u); /* decs bs=2 */
  CHECK(2, 0x60, 0x83c635c1u, 0xeab8d8beu, 0x38c635c1u); /* decs bs=3 */
  CHECK(2, 0x60, 0xc4436d49u, 0x94a92e3cu, 0x23436d49u); /* decs bs=3 */
  CHECK(2, 0x60, 0x9cb027ebu, 0x91ec726du, 0x30b027ebu); /* decs bs=3 */
  CHECK(3, 0x00, 0xeb9a6a1du, 0x7deab751u, 0xd69cb78bu); /* decsm bs=0 */
  CHECK(3, 0x00, 0x4b10d8f1u, 0x51d4ce36u, 0x1c9fc712u); /* decsm bs=0 */
  CHECK(3, 0x00, 0xca5f2ddfu, 0xd37f9f8du, 0xdada4e9du); /* decsm bs=0 */
  CHECK(3, 0x20, 0x314bd68au, 0x38ccdba7u, 0x70402da4u); /* decsm bs=1 */
  CHECK(3, 0x20, 0xd4b5251bu, 0x72d9df10u, 0x93634808u); /* decsm bs=1 */
  CHECK(3, 0x20, 0x6e448a2du, 0x1f2be00du, 0x0f9310a1u); /* decsm bs=1 */
  CHECK(3, 0x40, 0x06644298u, 0x48d7b5e3u, 0x63223dc9u); /* decsm bs=2 */
  CHECK(3, 0x40, 0x731ad016u, 0x0a1897dau, 0xfc193749u); /* decsm bs=2 */
  CHECK(3, 0x40, 0x0498aecbu, 0x39790b0bu, 0xa4584be1u); /* decsm bs=2 */
  CHECK(3, 0x60, 0x1b1487e7u, 0xf6ec6073u, 0x339fcedbu); /* decsm bs=3 */
  CHECK(3, 0x60, 0x26a59d42u, 0x34bf6f44u, 0x8d864e31u); /* decsm bs=3 */
  CHECK(3, 0x60, 0xf3a7fe0fu, 0x80a51fbcu, 0xa422e7feu); /* decsm bs=3 */
  puts(fails ? "FAIL\n" : "PASS\n");
  puts("checks="); puthex((uint32_t)total); puts(" fails="); puthex((uint32_t)fails); puts("\n");
  return fails ? 1 : 0;
}
