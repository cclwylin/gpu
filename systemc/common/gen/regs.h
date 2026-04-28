// ---------------------------------------------------------------------
// AUTO-GENERATED FROM specs/registers.yaml — DO NOT EDIT.
// Regenerate with: python3 tools/regmap_gen/regmap_gen.py
// ---------------------------------------------------------------------
#pragma once
#include <cstdint>

namespace gpu::regs {

// Bank CP: Command Processor state + ring buffer control
inline constexpr uint32_t CP_BASE = 0x00000000;
inline constexpr uint32_t CP_SIZE = 0x00001000;

inline constexpr uint32_t CP_CTRL       = 0x00000000;
inline constexpr uint32_t CP_CTRL_RESET = 0x00000000;
inline constexpr uint32_t CP_CTRL_ENABLE_LSB   = 0;
inline constexpr uint32_t CP_CTRL_ENABLE_WIDTH = 1;
inline constexpr uint32_t CP_CTRL_ENABLE_MASK  = 0x00000001;
inline constexpr uint32_t CP_CTRL_RESET_LSB   = 1;
inline constexpr uint32_t CP_CTRL_RESET_WIDTH = 1;
inline constexpr uint32_t CP_CTRL_RESET_MASK  = 0x00000002;
inline constexpr uint32_t CP_CTRL_HALT_LSB   = 2;
inline constexpr uint32_t CP_CTRL_HALT_WIDTH = 1;
inline constexpr uint32_t CP_CTRL_HALT_MASK  = 0x00000004;

inline constexpr uint32_t CP_RING_BASE       = 0x00000004;
inline constexpr uint32_t CP_RING_BASE_RESET = 0x00000000;
inline constexpr uint32_t CP_RING_BASE_ADDR_LSB   = 0;
inline constexpr uint32_t CP_RING_BASE_ADDR_WIDTH = 32;
inline constexpr uint32_t CP_RING_BASE_ADDR_MASK  = 0xFFFFFFFF;

inline constexpr uint32_t CP_RING_SIZE       = 0x00000008;
inline constexpr uint32_t CP_RING_SIZE_RESET = 0x00000000;
inline constexpr uint32_t CP_RING_SIZE_LOG2_LSB   = 0;
inline constexpr uint32_t CP_RING_SIZE_LOG2_WIDTH = 5;
inline constexpr uint32_t CP_RING_SIZE_LOG2_MASK  = 0x0000001F;

inline constexpr uint32_t CP_RING_HEAD       = 0x0000000C;
inline constexpr uint32_t CP_RING_HEAD_RESET = 0x00000000;
inline constexpr uint32_t CP_RING_HEAD_OFFSET_LSB   = 0;
inline constexpr uint32_t CP_RING_HEAD_OFFSET_WIDTH = 24;
inline constexpr uint32_t CP_RING_HEAD_OFFSET_MASK  = 0x00FFFFFF;

inline constexpr uint32_t CP_RING_TAIL       = 0x00000010;
inline constexpr uint32_t CP_RING_TAIL_RESET = 0x00000000;
inline constexpr uint32_t CP_RING_TAIL_OFFSET_LSB   = 0;
inline constexpr uint32_t CP_RING_TAIL_OFFSET_WIDTH = 24;
inline constexpr uint32_t CP_RING_TAIL_OFFSET_MASK  = 0x00FFFFFF;

inline constexpr uint32_t CP_STATUS       = 0x00000014;
inline constexpr uint32_t CP_STATUS_RESET = 0x00000000;
inline constexpr uint32_t CP_STATUS_IDLE_LSB   = 0;
inline constexpr uint32_t CP_STATUS_IDLE_WIDTH = 1;
inline constexpr uint32_t CP_STATUS_IDLE_MASK  = 0x00000001;
inline constexpr uint32_t CP_STATUS_STALL_LSB   = 1;
inline constexpr uint32_t CP_STATUS_STALL_WIDTH = 1;
inline constexpr uint32_t CP_STATUS_STALL_MASK  = 0x00000002;
inline constexpr uint32_t CP_STATUS_ERR_LSB   = 2;
inline constexpr uint32_t CP_STATUS_ERR_WIDTH = 1;
inline constexpr uint32_t CP_STATUS_ERR_MASK  = 0x00000004;

inline constexpr uint32_t CP_IRQ_STATUS       = 0x00000018;
inline constexpr uint32_t CP_IRQ_STATUS_RESET = 0x00000000;
inline constexpr uint32_t CP_IRQ_STATUS_DONE_LSB   = 0;
inline constexpr uint32_t CP_IRQ_STATUS_DONE_WIDTH = 1;
inline constexpr uint32_t CP_IRQ_STATUS_DONE_MASK  = 0x00000001;
inline constexpr uint32_t CP_IRQ_STATUS_ERR_LSB   = 1;
inline constexpr uint32_t CP_IRQ_STATUS_ERR_WIDTH = 1;
inline constexpr uint32_t CP_IRQ_STATUS_ERR_MASK  = 0x00000002;

// Bank MMU: MMU + TLB control
inline constexpr uint32_t MMU_BASE = 0x00001000;
inline constexpr uint32_t MMU_SIZE = 0x00001000;

inline constexpr uint32_t MMU_CTRL       = 0x00001000;
inline constexpr uint32_t MMU_CTRL_RESET = 0x00000000;
inline constexpr uint32_t MMU_CTRL_ENABLE_LSB   = 0;
inline constexpr uint32_t MMU_CTRL_ENABLE_WIDTH = 1;
inline constexpr uint32_t MMU_CTRL_ENABLE_MASK  = 0x00000001;
inline constexpr uint32_t MMU_CTRL_TLB_FLUSH_LSB   = 1;
inline constexpr uint32_t MMU_CTRL_TLB_FLUSH_WIDTH = 1;
inline constexpr uint32_t MMU_CTRL_TLB_FLUSH_MASK  = 0x00000002;

inline constexpr uint32_t MMU_PT_BASE       = 0x00001004;
inline constexpr uint32_t MMU_PT_BASE_RESET = 0x00000000;
inline constexpr uint32_t MMU_PT_BASE_ADDR_LSB   = 0;
inline constexpr uint32_t MMU_PT_BASE_ADDR_WIDTH = 32;
inline constexpr uint32_t MMU_PT_BASE_ADDR_MASK  = 0xFFFFFFFF;

inline constexpr uint32_t MMU_FAULT_ADDR       = 0x00001008;
inline constexpr uint32_t MMU_FAULT_ADDR_RESET = 0x00000000;
inline constexpr uint32_t MMU_FAULT_ADDR_VADDR_LSB   = 0;
inline constexpr uint32_t MMU_FAULT_ADDR_VADDR_WIDTH = 32;
inline constexpr uint32_t MMU_FAULT_ADDR_VADDR_MASK  = 0xFFFFFFFF;

inline constexpr uint32_t MMU_FAULT_STATUS       = 0x0000100C;
inline constexpr uint32_t MMU_FAULT_STATUS_RESET = 0x00000000;
inline constexpr uint32_t MMU_FAULT_STATUS_VALID_LSB   = 0;
inline constexpr uint32_t MMU_FAULT_STATUS_VALID_WIDTH = 1;
inline constexpr uint32_t MMU_FAULT_STATUS_VALID_MASK  = 0x00000001;
inline constexpr uint32_t MMU_FAULT_STATUS_TYPE_LSB   = 1;
inline constexpr uint32_t MMU_FAULT_STATUS_TYPE_WIDTH = 3;
inline constexpr uint32_t MMU_FAULT_STATUS_TYPE_MASK  = 0x0000000E;
inline constexpr uint32_t MMU_FAULT_STATUS_CLIENT_LSB   = 4;
inline constexpr uint32_t MMU_FAULT_STATUS_CLIENT_WIDTH = 4;
inline constexpr uint32_t MMU_FAULT_STATUS_CLIENT_MASK  = 0x000000F0;

// Bank FBO: Framebuffer + MSAA config
inline constexpr uint32_t FBO_BASE = 0x00002000;
inline constexpr uint32_t FBO_SIZE = 0x00001000;

inline constexpr uint32_t FBO_CTRL       = 0x00002000;
inline constexpr uint32_t FBO_CTRL_RESET = 0x00000000;
inline constexpr uint32_t FBO_CTRL_MSAA_EN_LSB   = 0;
inline constexpr uint32_t FBO_CTRL_MSAA_EN_WIDTH = 1;
inline constexpr uint32_t FBO_CTRL_MSAA_EN_MASK  = 0x00000001;
inline constexpr uint32_t FBO_CTRL_A2C_EN_LSB   = 1;
inline constexpr uint32_t FBO_CTRL_A2C_EN_WIDTH = 1;
inline constexpr uint32_t FBO_CTRL_A2C_EN_MASK  = 0x00000002;
inline constexpr uint32_t FBO_CTRL_RESOLVE_MODE_LSB   = 2;
inline constexpr uint32_t FBO_CTRL_RESOLVE_MODE_WIDTH = 2;
inline constexpr uint32_t FBO_CTRL_RESOLVE_MODE_MASK  = 0x0000000C;

inline constexpr uint32_t FBO_COLOR_BASE       = 0x00002004;
inline constexpr uint32_t FBO_COLOR_BASE_RESET = 0x00000000;
inline constexpr uint32_t FBO_COLOR_BASE_ADDR_LSB   = 0;
inline constexpr uint32_t FBO_COLOR_BASE_ADDR_WIDTH = 32;
inline constexpr uint32_t FBO_COLOR_BASE_ADDR_MASK  = 0xFFFFFFFF;

inline constexpr uint32_t FBO_DEPTH_BASE       = 0x00002008;
inline constexpr uint32_t FBO_DEPTH_BASE_RESET = 0x00000000;

inline constexpr uint32_t FBO_WIDTH       = 0x0000200C;
inline constexpr uint32_t FBO_WIDTH_RESET = 0x00000000;
inline constexpr uint32_t FBO_WIDTH_W_LSB   = 0;
inline constexpr uint32_t FBO_WIDTH_W_WIDTH = 14;
inline constexpr uint32_t FBO_WIDTH_W_MASK  = 0x00003FFF;

inline constexpr uint32_t FBO_HEIGHT       = 0x00002010;
inline constexpr uint32_t FBO_HEIGHT_RESET = 0x00000000;
inline constexpr uint32_t FBO_HEIGHT_H_LSB   = 0;
inline constexpr uint32_t FBO_HEIGHT_H_WIDTH = 14;
inline constexpr uint32_t FBO_HEIGHT_H_MASK  = 0x00003FFF;

// Bank TBF: Tile buffer configuration
inline constexpr uint32_t TBF_BASE = 0x00003000;
inline constexpr uint32_t TBF_SIZE = 0x00001000;

inline constexpr uint32_t TBF_CTRL       = 0x00003000;
inline constexpr uint32_t TBF_CTRL_RESET = 0x00000000;
inline constexpr uint32_t TBF_CTRL_BANK_CFG_LSB   = 0;
inline constexpr uint32_t TBF_CTRL_BANK_CFG_WIDTH = 3;
inline constexpr uint32_t TBF_CTRL_BANK_CFG_MASK  = 0x00000007;
inline constexpr uint32_t TBF_CTRL_BIST_START_LSB   = 8;
inline constexpr uint32_t TBF_CTRL_BIST_START_WIDTH = 1;
inline constexpr uint32_t TBF_CTRL_BIST_START_MASK  = 0x00000100;

inline constexpr uint32_t TBF_STATUS       = 0x00003004;
inline constexpr uint32_t TBF_STATUS_RESET = 0x00000000;
inline constexpr uint32_t TBF_STATUS_BIST_DONE_LSB   = 0;
inline constexpr uint32_t TBF_STATUS_BIST_DONE_WIDTH = 1;
inline constexpr uint32_t TBF_STATUS_BIST_DONE_MASK  = 0x00000001;
inline constexpr uint32_t TBF_STATUS_BIST_FAIL_LSB   = 1;
inline constexpr uint32_t TBF_STATUS_BIST_FAIL_WIDTH = 1;
inline constexpr uint32_t TBF_STATUS_BIST_FAIL_MASK  = 0x00000002;

// Bank SHADER: Shader core configuration
inline constexpr uint32_t SHADER_BASE = 0x00004000;
inline constexpr uint32_t SHADER_SIZE = 0x00001000;

inline constexpr uint32_t SHADER_VS_BASE       = 0x00004000;
inline constexpr uint32_t SHADER_VS_BASE_RESET = 0x00000000;
inline constexpr uint32_t SHADER_VS_BASE_ADDR_LSB   = 0;
inline constexpr uint32_t SHADER_VS_BASE_ADDR_WIDTH = 32;
inline constexpr uint32_t SHADER_VS_BASE_ADDR_MASK  = 0xFFFFFFFF;

inline constexpr uint32_t SHADER_FS_BASE       = 0x00004004;
inline constexpr uint32_t SHADER_FS_BASE_RESET = 0x00000000;

inline constexpr uint32_t SHADER_SCRATCH_BASE       = 0x00004008;
inline constexpr uint32_t SHADER_SCRATCH_BASE_RESET = 0x00000000;

inline constexpr uint32_t SHADER_SCRATCH_SIZE       = 0x0000400C;
inline constexpr uint32_t SHADER_SCRATCH_SIZE_RESET = 0x00000000;

inline constexpr uint32_t SHADER_UBO_BASE       = 0x00004010;
inline constexpr uint32_t SHADER_UBO_BASE_RESET = 0x00000000;

// Bank TEX: Texture binding slot table (16 slots x 16 B)
inline constexpr uint32_t TEX_BASE = 0x00005000;
inline constexpr uint32_t TEX_SIZE = 0x00001000;

inline constexpr uint32_t TEX0_BASE       = 0x00005000;
inline constexpr uint32_t TEX0_BASE_RESET = 0x00000000;
inline constexpr uint32_t TEX0_BASE_ADDR_LSB   = 0;
inline constexpr uint32_t TEX0_BASE_ADDR_WIDTH = 32;
inline constexpr uint32_t TEX0_BASE_ADDR_MASK  = 0xFFFFFFFF;

inline constexpr uint32_t TEX0_CFG       = 0x00005004;
inline constexpr uint32_t TEX0_CFG_RESET = 0x00000000;
inline constexpr uint32_t TEX0_CFG_FORMAT_LSB   = 0;
inline constexpr uint32_t TEX0_CFG_FORMAT_WIDTH = 4;
inline constexpr uint32_t TEX0_CFG_FORMAT_MASK  = 0x0000000F;
inline constexpr uint32_t TEX0_CFG_WRAP_S_LSB   = 4;
inline constexpr uint32_t TEX0_CFG_WRAP_S_WIDTH = 2;
inline constexpr uint32_t TEX0_CFG_WRAP_S_MASK  = 0x00000030;
inline constexpr uint32_t TEX0_CFG_WRAP_T_LSB   = 6;
inline constexpr uint32_t TEX0_CFG_WRAP_T_WIDTH = 2;
inline constexpr uint32_t TEX0_CFG_WRAP_T_MASK  = 0x000000C0;
inline constexpr uint32_t TEX0_CFG_FILTER_MIN_LSB   = 8;
inline constexpr uint32_t TEX0_CFG_FILTER_MIN_WIDTH = 3;
inline constexpr uint32_t TEX0_CFG_FILTER_MIN_MASK  = 0x00000700;
inline constexpr uint32_t TEX0_CFG_FILTER_MAG_LSB   = 11;
inline constexpr uint32_t TEX0_CFG_FILTER_MAG_WIDTH = 2;
inline constexpr uint32_t TEX0_CFG_FILTER_MAG_MASK  = 0x00001800;
inline constexpr uint32_t TEX0_CFG_MIP_ENABLE_LSB   = 13;
inline constexpr uint32_t TEX0_CFG_MIP_ENABLE_WIDTH = 1;
inline constexpr uint32_t TEX0_CFG_MIP_ENABLE_MASK  = 0x00002000;

inline constexpr uint32_t TEX0_DIM       = 0x00005008;
inline constexpr uint32_t TEX0_DIM_RESET = 0x00000000;
inline constexpr uint32_t TEX0_DIM_WIDTH_LSB   = 0;
inline constexpr uint32_t TEX0_DIM_WIDTH_WIDTH = 14;
inline constexpr uint32_t TEX0_DIM_WIDTH_MASK  = 0x00003FFF;
inline constexpr uint32_t TEX0_DIM_HEIGHT_LSB   = 16;
inline constexpr uint32_t TEX0_DIM_HEIGHT_WIDTH = 14;
inline constexpr uint32_t TEX0_DIM_HEIGHT_MASK  = 0x3FFF0000;

inline constexpr uint32_t TEX0_MIP       = 0x0000500C;
inline constexpr uint32_t TEX0_MIP_RESET = 0x00000000;
inline constexpr uint32_t TEX0_MIP_LOG2_LEVELS_LSB   = 0;
inline constexpr uint32_t TEX0_MIP_LOG2_LEVELS_WIDTH = 4;
inline constexpr uint32_t TEX0_MIP_LOG2_LEVELS_MASK  = 0x0000000F;

// Bank PMU: Performance counters + trace
inline constexpr uint32_t PMU_BASE = 0x00010000;
inline constexpr uint32_t PMU_SIZE = 0x00010000;

inline constexpr uint32_t PMU_CTRL       = 0x00010000;
inline constexpr uint32_t PMU_CTRL_RESET = 0x00000000;
inline constexpr uint32_t PMU_CTRL_ENABLE_LSB   = 0;
inline constexpr uint32_t PMU_CTRL_ENABLE_WIDTH = 1;
inline constexpr uint32_t PMU_CTRL_ENABLE_MASK  = 0x00000001;
inline constexpr uint32_t PMU_CTRL_RESET_LSB   = 1;
inline constexpr uint32_t PMU_CTRL_RESET_WIDTH = 1;
inline constexpr uint32_t PMU_CTRL_RESET_MASK  = 0x00000002;

}  // namespace gpu::regs
