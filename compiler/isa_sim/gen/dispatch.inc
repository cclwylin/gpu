// ---------------------------------------------------------------------
// AUTO-GENERATED FROM specs/isa.yaml — DO NOT EDIT.
// Regenerate with: python3 tools/isa_gen/isa_gen.py
// ---------------------------------------------------------------------
// Included by compiler/isa_sim/executor.cpp inside switch(op).

case Opcode::OP_NOP:
    exec_nop(ctx, inst);
    break;
case Opcode::OP_MOV:
    exec_mov(ctx, inst);
    break;
case Opcode::OP_ADD:
    exec_add(ctx, inst);
    break;
case Opcode::OP_MUL:
    exec_mul(ctx, inst);
    break;
case Opcode::OP_DP2:
    exec_dp2(ctx, inst);
    break;
case Opcode::OP_DP3:
    exec_dp3(ctx, inst);
    break;
case Opcode::OP_DP4:
    exec_dp4(ctx, inst);
    break;
case Opcode::OP_RCP:
    exec_rcp(ctx, inst);
    break;
case Opcode::OP_RSQ:
    exec_rsq(ctx, inst);
    break;
case Opcode::OP_EXP:
    exec_exp(ctx, inst);
    break;
case Opcode::OP_LOG:
    exec_log(ctx, inst);
    break;
case Opcode::OP_SIN:
    exec_sin(ctx, inst);
    break;
case Opcode::OP_COS:
    exec_cos(ctx, inst);
    break;
case Opcode::OP_MIN:
    exec_min(ctx, inst);
    break;
case Opcode::OP_MAX:
    exec_max(ctx, inst);
    break;
case Opcode::OP_ABS:
    exec_abs(ctx, inst);
    break;
case Opcode::OP_FRC:
    exec_frc(ctx, inst);
    break;
case Opcode::OP_FLR:
    exec_flr(ctx, inst);
    break;
case Opcode::OP_MAD:
    exec_mad(ctx, inst);
    break;
case Opcode::OP_CMP:
    exec_cmp(ctx, inst);
    break;
case Opcode::OP_SETP_EQ:
    exec_setp_eq(ctx, inst);
    break;
case Opcode::OP_SETP_NE:
    exec_setp_ne(ctx, inst);
    break;
case Opcode::OP_SETP_LT:
    exec_setp_lt(ctx, inst);
    break;
case Opcode::OP_SETP_LE:
    exec_setp_le(ctx, inst);
    break;
case Opcode::OP_SETP_GT:
    exec_setp_gt(ctx, inst);
    break;
case Opcode::OP_SETP_GE:
    exec_setp_ge(ctx, inst);
    break;
case Opcode::OP_BRA:
    exec_bra(ctx, inst);
    break;
case Opcode::OP_CALL:
    exec_call(ctx, inst);
    break;
case Opcode::OP_RET:
    exec_ret(ctx, inst);
    break;
case Opcode::OP_LOOP:
    exec_loop(ctx, inst);
    break;
case Opcode::OP_ENDLOOP:
    exec_endloop(ctx, inst);
    break;
case Opcode::OP_BREAK:
    exec_break(ctx, inst);
    break;
case Opcode::OP_IF_P:
    exec_if_p(ctx, inst);
    break;
case Opcode::OP_ELSE:
    exec_else(ctx, inst);
    break;
case Opcode::OP_ENDIF:
    exec_endif(ctx, inst);
    break;
case Opcode::OP_KIL:
    exec_kil(ctx, inst);
    break;
case Opcode::OP_LD:
    exec_ld(ctx, inst);
    break;
case Opcode::OP_ST:
    exec_st(ctx, inst);
    break;
case Opcode::OP_TEX:
    exec_tex(ctx, inst);
    break;
case Opcode::OP_TEXB:
    exec_texb(ctx, inst);
    break;
case Opcode::OP_TEXL:
    exec_texl(ctx, inst);
    break;
case Opcode::OP_TEXG:
    exec_texg(ctx, inst);
    break;
