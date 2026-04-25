# Shader Corpus

Shader test bench,兩個用途:
1. Phase 0 ISA 表達力驗證(`ref_shader_1..3`)
2. Phase 1+ compiler / ISA sim / HW regression(target ~500 shader)

## Phase 0 Reference Shaders

三個手寫壓測 shader,覆蓋 ISA 全部能力,驗證 spec v0.1 足夠表達真實 workload:

| Shader | Focus | ISA coverage |
|---|---|---|
| [ref_shader_1](ref_shader_1/) | Gouraud + textured quad | ALU basics, texture, varying |
| [ref_shader_2](ref_shader_2/) | Phong per-fragment lighting | rsq, log/exp, mad-heavy, 3-op |
| [ref_shader_3](ref_shader_3/) | Circle discard + iterative loop | predicate, loop/break, kil |

驗證結果 → [ISA_VALIDATION.md](ISA_VALIDATION.md)

## Phase 1+ Large Corpus

目標 ~500 shader,來源:
- 真實 app shader(glmark2、opensource engine)
- 合成 shader(`tools/scene_gen` 參數化生成)

Phase 1 exit:100% compile + ISA sim = sw_ref。
