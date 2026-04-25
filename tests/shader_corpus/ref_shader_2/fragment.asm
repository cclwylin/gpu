; ref_shader_2 FS — Phong lighting
; ISA v1.0  (uses src negate modifier, mad 3-operand)
;
; ABI:
;   v0 = v_wpos.xyz
;   v1 = v_normal.xyz
;   c0 = u_light_pos.xyz
;   c1 = u_view_pos.xyz
;   c2 = u_base_color.xyz
;   c3 = u_spec_color.xyz
;   c4 = (u_shininess, _, _, _)   shininess in .x
;   c5 = (0.0, 1.0, 2.0, 0.5)     constants helper
;   o0 = gl_FragColor

; ---- N = normalize(v_normal) ----
dp3  r0.x, v1.xyz, v1.xyz       ; |n|^2 broadcast
rsq  r0.x, r0.x                 ; 1/|n|
mul  r1,   v1,     r0.xxxx      ; N

; ---- L = normalize(u_light_pos - v_wpos) ----
add  r2,   c0,    -v0           ; light - wpos (src1 negate)
dp3  r0.x, r2.xyz, r2.xyz
rsq  r0.x, r0.x
mul  r3,   r2,     r0.xxxx      ; L

; ---- V = normalize(u_view_pos - v_wpos) ----
add  r4,   c1,    -v0
dp3  r0.x, r4.xyz, r4.xyz
rsq  r0.x, r0.x
mul  r5,   r4,     r0.xxxx      ; V

; ---- R = 2*dot(N,L)*N - L ----
dp3  r0.x, r1.xyz, r3.xyz       ; dot(N, L)
add  r0.x, r0.x,   r0.x         ; 2*dot
mad  r6,   r1,     r0.xxxx, -r3 ; N * 2dot - L

; ---- ndotl = max(dot(N,L), 0) ----
dp3  r0.x, r1.xyz, r3.xyz
max  r0.x, r0.x,   c5.xxxx      ; c5.x = 0.0

; ---- rdotv = max(dot(R,V), 0) ----
dp3  r0.y, r6.xyz, r5.xyz
max  r0.y, r0.y,   c5.xxxx

; ---- spec = pow(rdotv, shin) = exp(log(rdotv) * shin) ----
log  r0.z, r0.yyyy
mul  r0.z, r0.zzzz, c4.xxxx
exp  r0.z, r0.zzzz

; ---- color = base*ndotl + spec_color*spec, alpha = 1 ----
mul  r7,   c2,    r0.xxxx       ; base * ndotl
mad  r7,   c3,    r0.zzzz, r7   ; + spec * spec_color

mov  o0.xyz, r7.xyz
mov  o0.w,   c5.yyyy            ; 1.0
