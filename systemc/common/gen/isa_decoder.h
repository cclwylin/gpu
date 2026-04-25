// ---------------------------------------------------------------------
// AUTO-GENERATED FROM specs/isa.yaml — DO NOT EDIT.
// Regenerate with: python3 tools/isa_gen/isa_gen.py
// ---------------------------------------------------------------------
#pragma once
#include <cstdint>

namespace gpu::isa::decoder {

inline constexpr uint32_t OPC_NOP = 0x00;
inline constexpr uint32_t OPC_MOV = 0x01;
inline constexpr uint32_t OPC_ADD = 0x02;
inline constexpr uint32_t OPC_MUL = 0x03;
inline constexpr uint32_t OPC_MAD = 0x04;
inline constexpr uint32_t OPC_DP3 = 0x05;
inline constexpr uint32_t OPC_DP4 = 0x06;
inline constexpr uint32_t OPC_RCP = 0x07;
inline constexpr uint32_t OPC_RSQ = 0x08;
inline constexpr uint32_t OPC_EXP = 0x09;
inline constexpr uint32_t OPC_LOG = 0x0A;
inline constexpr uint32_t OPC_SIN = 0x0B;
inline constexpr uint32_t OPC_COS = 0x0C;
inline constexpr uint32_t OPC_MIN = 0x0D;
inline constexpr uint32_t OPC_MAX = 0x0E;
inline constexpr uint32_t OPC_ABS = 0x0F;
inline constexpr uint32_t OPC_FRC = 0x10;
inline constexpr uint32_t OPC_FLR = 0x11;
inline constexpr uint32_t OPC_CMP = 0x12;
inline constexpr uint32_t OPC_BRA = 0x20;
inline constexpr uint32_t OPC_CALL = 0x21;
inline constexpr uint32_t OPC_RET = 0x22;
inline constexpr uint32_t OPC_LOOP = 0x23;
inline constexpr uint32_t OPC_ENDLOOP = 0x24;
inline constexpr uint32_t OPC_BREAK = 0x25;
inline constexpr uint32_t OPC_KIL = 0x26;
inline constexpr uint32_t OPC_LD = 0x30;
inline constexpr uint32_t OPC_ST = 0x31;
inline constexpr uint32_t OPC_TEX = 0x34;
inline constexpr uint32_t OPC_TEXB = 0x35;
inline constexpr uint32_t OPC_TEXL = 0x36;
inline constexpr uint32_t OPC_TEXG = 0x37;

}  // namespace gpu::isa::decoder
