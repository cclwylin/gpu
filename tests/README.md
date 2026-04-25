# tests/

```
scenes/           程式化產生的測試 scene(non-CTS)
shader_corpus/    ~500 GLSL shaders(真實 app + 合成)
conformance/      OpenGL ES 2.0 CTS subset + dEQP MSAA subset
regressions/      每次 bug fix 留下的 minimal repro
```

## Scene Types
- smoke:10 個,每 commit 跑,5 分鐘內完成
- full:50 個,nightly 跑
- stress:大 draw、large tex、long shader,weekly

## Shader Corpus
- glslang 介接 open source shader collection
- 合成 shader 程式化生成(不同 varying 數、分支深度、tex 數)
- Phase 1 exit 要 100% compile + ISA sim 結果 = sw_ref

## Conformance
- Phase 1 起持續跑
- ES 2.0 CTS subset(Khronos open source test suite)
- dEQP MSAA path

## Regression Policy
- 每個 close 的 bug 必須留一個 minimal repro 在 regressions/
- 命名:`<date>_<bug-id>_<short-desc>`
