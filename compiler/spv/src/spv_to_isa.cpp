// Sprint-13 SPIR-V → ISA lowering. See spv_to_isa.h for scope.

#include "gpu_spv/spv_to_isa.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace gpu::spv {
namespace {

using namespace gpu::isa;

// Subset of SPIR-V opcodes we recognise. Values from the spec.
enum SpvOp : uint16_t {
    OpName              = 5,
    OpTypeVoid          = 19,
    OpTypeFloat         = 22,
    OpTypeVector        = 23,
    OpTypeMatrix        = 24,
    OpTypePointer       = 32,
    OpTypeFunction      = 33,
    OpConstant          = 43,
    OpConstantComposite = 44,
    OpFunction          = 54,
    OpFunctionEnd       = 56,
    OpVariable          = 59,
    OpLoad              = 61,
    OpStore             = 62,
    OpDecorate          = 71,
    OpFAdd              = 129,
    OpFMul              = 133,
    OpMatrixTimesVector = 145,
    OpReturn            = 253,
    OpLabel             = 248,
};
enum SpvStorageClass : uint16_t {
    SC_UniformConstant = 0, SC_Input = 1, SC_Uniform = 2, SC_Output = 3, SC_Function = 7,
};
enum SpvDecoration : uint16_t {
    Dec_BuiltIn = 11, Dec_Location = 30, Dec_Binding = 33,
};
enum { BuiltIn_Position = 0 };

struct TypeInfo {
    enum K { VOID_, FLOAT, VEC, MAT, POINTER, FUNCTION } kind;
    int width = 0;
    uint32_t base_id = 0;
    SpvStorageClass storage = SC_Function;
};
struct VarInfo {
    uint32_t        type_ptr_id = 0;
    SpvStorageClass storage = SC_Function;
    std::string     name;
    int  location = -1, binding = -1;
    bool is_position = false;
};
struct ResultRef {
    enum K { NONE, GPR } kind = NONE;
    uint8_t  gpr = 0;
    uint32_t type_id = 0;
};

struct Lower {
    const std::vector<uint32_t>& words;
    Stage stage;
    LowerResult& out;

    std::unordered_map<uint32_t, TypeInfo>  types;
    std::unordered_map<uint32_t, VarInfo>   vars;
    std::unordered_map<uint32_t, ResultRef> results;
    std::unordered_map<uint32_t, std::string> names;

    int next_attr = 0, next_const = 0, next_vsout = 1;
    int next_fsin = 0, next_sampler = 0, next_gpr = 0;

    std::unordered_map<uint32_t, int> attr_slot, varyout_slot, varyin_slot, sampler_slot;
    std::unordered_map<uint32_t, int> uniform_slot;
    std::unordered_map<uint32_t, bool> var_is_position;
    std::unordered_map<uint32_t, int> mat_uniform_for_load;

    Lower(const std::vector<uint32_t>& w, Stage s, LowerResult& o)
        : words(w), stage(s), out(o) {}

    int alloc_gpr() { return next_gpr++; }

    void emit_mov(uint8_t dst_class, uint8_t dst_idx, uint8_t wmsk,
                  uint8_t s0c, uint8_t s0idx, uint8_t sw0) {
        AluFields f{};
        f.op = 0x01; f.pmd = PMD_ALWAYS;
        f.dst = dst_idx; f.dst_class = dst_class; f.wmsk = wmsk;
        f.s0c = s0c; f.s0idx = s0idx; f.sw0 = sw0;
        out.code.push_back(encode_alu(f));
    }
    void emit_bin(uint8_t op, uint8_t dst_idx, uint8_t wmsk,
                  uint8_t s0c, uint8_t s0idx, uint8_t sw0,
                  uint8_t s1c, uint8_t s1idx, uint8_t sw1) {
        AluFields f{};
        f.op = op; f.pmd = PMD_ALWAYS;
        f.dst = dst_idx; f.dst_class = 0; f.wmsk = wmsk;
        f.s0c = s0c; f.s0idx = s0idx; f.sw0 = sw0;
        f.s1c = s1c; f.s1idx = s1idx; f.sw1 = sw1;
        out.code.push_back(encode_alu(f));
    }
    bool err(const char* m) { out.error = m; return false; }

    bool scan_module() {
        if (words.size() < 5 || words[0] != 0x07230203u) return err("bad SPIR-V header");
        size_t i = 5;
        while (i < words.size()) {
            uint32_t w0 = words[i];
            uint16_t op  = static_cast<uint16_t>(w0 & 0xFFFF);
            uint16_t len = static_cast<uint16_t>(w0 >> 16);
            if (len == 0 || i + len > words.size()) return err("malformed SPIR-V stream");
            const uint32_t* arg = &words[i + 1];

            switch (op) {
                case OpName:
                    if (len > 1) names[arg[0]] = std::string(reinterpret_cast<const char*>(&arg[1]));
                    break;
                case OpTypeVoid:    types[arg[0]] = {TypeInfo::VOID_, 0, 0, SC_Function}; break;
                case OpTypeFloat:   types[arg[0]] = {TypeInfo::FLOAT, static_cast<int>(arg[1]), 0, SC_Function}; break;
                case OpTypeVector:  types[arg[0]] = {TypeInfo::VEC,   static_cast<int>(arg[2]), arg[1], SC_Function}; break;
                case OpTypeMatrix:  types[arg[0]] = {TypeInfo::MAT,   static_cast<int>(arg[2]), arg[1], SC_Function}; break;
                case OpTypePointer: types[arg[0]] = {TypeInfo::POINTER, 0, arg[2], static_cast<SpvStorageClass>(arg[1])}; break;
                case OpTypeFunction:types[arg[0]] = {TypeInfo::FUNCTION, 0, arg[1], SC_Function}; break;
                case OpVariable: {
                    VarInfo v{}; v.type_ptr_id = arg[0]; v.storage = static_cast<SpvStorageClass>(arg[2]);
                    auto nit = names.find(arg[1]); if (nit != names.end()) v.name = nit->second;
                    vars[arg[1]] = v;
                    break;
                }
                case OpDecorate: {
                    uint32_t id = arg[0];
                    auto vit = vars.find(id);
                    if (vit == vars.end()) break;
                    SpvDecoration d = static_cast<SpvDecoration>(arg[1]);
                    if (d == Dec_Location && len >= 4)  vit->second.location = static_cast<int>(arg[2]);
                    else if (d == Dec_Binding && len >= 4) vit->second.binding = static_cast<int>(arg[2]);
                    else if (d == Dec_BuiltIn && len >= 4 && arg[2] == BuiltIn_Position)
                        vit->second.is_position = true;
                    break;
                }
                default: break;
            }
            i += len;
        }
        return true;
    }

    bool bind_variables() {
        std::vector<uint32_t> ids; ids.reserve(vars.size());
        for (auto& kv : vars) ids.push_back(kv.first);
        std::sort(ids.begin(), ids.end());

        for (uint32_t id : ids) {
            VarInfo& v = vars[id];
            auto pt = types.find(v.type_ptr_id);
            if (pt == types.end()) continue;
            uint32_t pointee = pt->second.base_id;
            auto et = types.find(pointee);
            std::string ts = "?";
            if (et != types.end()) {
                if      (et->second.kind == TypeInfo::FLOAT) ts = "float";
                else if (et->second.kind == TypeInfo::VEC)   ts = "vec" + std::to_string(et->second.width);
                else if (et->second.kind == TypeInfo::MAT)   ts = "mat" + std::to_string(et->second.width);
            }
            const std::string& nm = v.name.empty() ? std::string("_") : v.name;
            switch (v.storage) {
                case SC_Input:
                    if (stage == Stage::Vertex) {
                        attr_slot[id] = next_attr;
                        out.attributes.push_back({nm, ts, next_attr++});
                    } else {
                        varyin_slot[id] = next_fsin;
                        out.varyings_in.push_back({nm, ts, next_fsin++});
                    }
                    break;
                case SC_Output:
                    if (v.is_position || stage == Stage::Fragment) {
                        var_is_position[id] = true;
                    } else {
                        varyout_slot[id] = next_vsout;
                        out.varyings_out.push_back({nm, ts, next_vsout++});
                    }
                    break;
                case SC_UniformConstant:
                case SC_Uniform:
                    if (et != types.end() && et->second.kind == TypeInfo::MAT) {
                        uniform_slot[id] = next_const;
                        out.uniforms.push_back({nm, ts, next_const});
                        next_const += 4;
                    } else {
                        uniform_slot[id] = next_const;
                        out.uniforms.push_back({nm, ts, next_const++});
                    }
                    break;
                default: break;
            }
        }
        return true;
    }

    void note_matrix_loads() {
        size_t i = 5; bool in_func = false;
        while (i < words.size()) {
            uint32_t w0 = words[i];
            uint16_t op  = static_cast<uint16_t>(w0 & 0xFFFF);
            uint16_t len = static_cast<uint16_t>(w0 >> 16);
            const uint32_t* arg = &words[i + 1];
            if (op == OpFunction)    in_func = true;
            if (op == OpFunctionEnd) in_func = false;
            if (in_func && op == OpLoad) {
                uint32_t result = arg[1];
                uint32_t ptr_id = arg[2];
                auto vit = vars.find(ptr_id);
                if (vit != vars.end()) {
                    auto pt = types.find(vit->second.type_ptr_id);
                    if (pt != types.end()) {
                        auto et = types.find(pt->second.base_id);
                        if (et != types.end() && et->second.kind == TypeInfo::MAT) {
                            auto sit = uniform_slot.find(ptr_id);
                            if (sit != uniform_slot.end())
                                mat_uniform_for_load[result] = sit->second;
                        }
                    }
                }
            }
            i += len;
        }
    }

    bool emit_function() {
        size_t i = 5; bool in_func = false;
        while (i < words.size()) {
            uint32_t w0 = words[i];
            uint16_t op  = static_cast<uint16_t>(w0 & 0xFFFF);
            uint16_t len = static_cast<uint16_t>(w0 >> 16);
            const uint32_t* arg = &words[i + 1];
            if (op == OpFunction)    { in_func = true;  i += len; continue; }
            if (op == OpFunctionEnd) { in_func = false; i += len; continue; }
            if (!in_func)            {                   i += len; continue; }

            switch (op) {
                case OpLabel:
                case OpReturn:
                    break;
                case OpVariable: {
                    int g = alloc_gpr();
                    ResultRef r; r.kind = ResultRef::GPR;
                    r.gpr = static_cast<uint8_t>(g); r.type_id = arg[0];
                    results[arg[1]] = r;
                    break;
                }
                case OpLoad: {
                    uint32_t result = arg[1];
                    uint32_t ptr_id = arg[2];
                    auto vit = vars.find(ptr_id);
                    if (vit != vars.end()) {
                        const VarInfo& v = vit->second;
                        uint8_t cls=SRC_GPR, idx=0;
                        if (v.storage == SC_Input && stage == Stage::Vertex) {
                            cls = SRC_GPR; idx = static_cast<uint8_t>(attr_slot[ptr_id]);
                        } else if (v.storage == SC_Input) {
                            cls = SRC_VARYING; idx = static_cast<uint8_t>(varyin_slot[ptr_id]);
                        } else if (v.storage == SC_Uniform || v.storage == SC_UniformConstant) {
                            cls = SRC_CONST; idx = static_cast<uint8_t>(uniform_slot[ptr_id]);
                        } else {
                            return err("OpLoad: unsupported storage class");
                        }
                        int g = alloc_gpr();
                        emit_mov(0, encode_dst_gpr(g), 0xF, cls, idx, SWIZZLE_IDENTITY);
                        ResultRef r; r.kind = ResultRef::GPR;
                        r.gpr = static_cast<uint8_t>(g); r.type_id = arg[0];
                        results[result] = r;
                        break;
                    }
                    auto pr = results.find(ptr_id);
                    if (pr != results.end()) { ResultRef r = pr->second; r.type_id = arg[0]; results[result] = r; break; }
                    return err("OpLoad: unknown source");
                }
                case OpStore: {
                    uint32_t ptr_id = arg[0];
                    uint32_t val_id = arg[1];
                    auto vrit = results.find(val_id);
                    if (vrit == results.end()) return err("OpStore: rhs not in GPR");
                    uint8_t v_idx = vrit->second.gpr;
                    auto vit = vars.find(ptr_id);
                    if (vit == vars.end()) return err("OpStore: unknown var");
                    if (vit->second.storage != SC_Output) return err("OpStore: target not Output");
                    if (var_is_position[ptr_id] || stage == Stage::Fragment) {
                        emit_mov(1, encode_dst_out(0), 0xF, SRC_GPR, v_idx, SWIZZLE_IDENTITY);
                    } else {
                        int slot = varyout_slot[ptr_id];
                        emit_mov(1, encode_dst_out(static_cast<uint8_t>(slot)), 0xF,
                                 SRC_GPR, v_idx, SWIZZLE_IDENTITY);
                    }
                    break;
                }
                case OpFMul:
                case OpFAdd: {
                    uint32_t result = arg[1];
                    auto a = results.find(arg[2]);
                    auto b = results.find(arg[3]);
                    if (a == results.end() || b == results.end())
                        return err("OpFMul/Add: missing operand");
                    int g = alloc_gpr();
                    uint8_t ot = (op == OpFMul) ? 0x03 : 0x02;
                    emit_bin(ot, encode_dst_gpr(g), 0xF,
                             SRC_GPR, a->second.gpr, SWIZZLE_IDENTITY,
                             SRC_GPR, b->second.gpr, SWIZZLE_IDENTITY);
                    ResultRef r; r.kind = ResultRef::GPR;
                    r.gpr = static_cast<uint8_t>(g); r.type_id = arg[0];
                    results[result] = r;
                    break;
                }
                case OpMatrixTimesVector: {
                    uint32_t result = arg[1];
                    uint32_t mat_id = arg[2];
                    uint32_t vec_id = arg[3];
                    auto mit = mat_uniform_for_load.find(mat_id);
                    if (mit == mat_uniform_for_load.end())
                        return err("OpMatrixTimesVector: matrix not a tracked uniform");
                    auto vrit = results.find(vec_id);
                    if (vrit == results.end()) return err("OpMatrixTimesVector: vec not in GPR");
                    int mat_c_base = mit->second;
                    uint8_t v_idx = vrit->second.gpr;
                    int g = alloc_gpr();
                    for (int row = 0; row < 4; ++row) {
                        AluFields f{};
                        f.op = 0x06;     // dp4
                        f.pmd = PMD_ALWAYS;
                        f.dst = encode_dst_gpr(g); f.dst_class = 0;
                        f.wmsk = static_cast<uint8_t>(1 << row);
                        f.s0c = SRC_CONST; f.s0idx = static_cast<uint8_t>(mat_c_base + row);
                        f.sw0 = SWIZZLE_IDENTITY;
                        f.s1c = SRC_GPR; f.s1idx = v_idx; f.sw1 = SWIZZLE_IDENTITY;
                        out.code.push_back(encode_alu(f));
                    }
                    ResultRef r; r.kind = ResultRef::GPR;
                    r.gpr = static_cast<uint8_t>(g); r.type_id = arg[0];
                    results[result] = r;
                    break;
                }
                default:
                    break;
            }
            i += len;
        }
        return true;
    }
};

}  // namespace

LowerResult lower(const std::vector<uint32_t>& spirv, Stage stage) {
    LowerResult res;
    Lower L(spirv, stage, res);
    if (!L.scan_module())    return res;
    if (!L.bind_variables()) return res;
    L.note_matrix_loads();
    L.emit_function();
    return res;
}

}  // namespace gpu::spv
