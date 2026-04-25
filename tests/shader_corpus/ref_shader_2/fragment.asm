; ref_shader_2 FS — Phong lighting
;
; ABI:
;   v0 = v_wpos.xyz    (w undef)
;   v1 = v_normal.xyz
;   c0 = u_light_pos   (xyz)
;   c1 = u_view_pos    (xyz)
;   c2 = u_base_color
;   c3 = u_spec_color
;   c4 = (u_shininess, 0, 0, 0)   -- scalar in .x
;   c5 = (0.0, 1.0, -1.0, 0.0)    -- constants helper
;   o0 = gl_FragColor
;
; Strategy:
;   pow(x, s) lowered to:  exp(log(x) * s)   -- uses log, mul, exp

; ----- N = normalize(v_normal) -----
dp3  r0.x,  v1, v1            ; |n|^2 broadcast
rsq  r0.x,  r0.x              ; 1/|n|
mul  r1,    v1, r0.xxxx       ; N = normal * 1/|n|    (r1.xyz)

; ----- L = normalize(u_light_pos - v_wpos) -----
add  r2,    c0, -v0           ; r2 = light - wpos   (需支援 src negate modifier)
dp3  r0.x,  r2, r2
rsq  r0.x,  r0.x
mul  r3,    r2, r0.xxxx       ; L = r3.xyz

; ----- V = normalize(u_view_pos - v_wpos) -----
add  r4,    c1, -v0
dp3  r0.x,  r4, r4
rsq  r0.x,  r0.x
mul  r5,    r4, r0.xxxx       ; V = r5.xyz

; ----- R = reflect(-L, N) = -L - 2 * dot(-L, N) * N
;                          =  2 * dot(L, N) * N - L
dp3  r0.x,  r3, r1            ; dot(L, N)
add  r0.x,  r0.x, r0.x        ; 2 * dot
mad  r6,    r1, r0.xxxx, -r3  ; R = N * 2dot - L

; ----- ndotl = max(dot(N, L), 0) -----
dp3  r0.x,  r1, r3
max  r0.x,  r0.x, c5.xxxx     ; c5.x = 0.0

; ----- rdotv = max(dot(R, V), 0) -----
dp3  r0.y,  r6, r5
max  r0.y,  r0.y, c5.xxxx

; ----- spec = pow(rdotv, shininess) = exp(log(rdotv) * shininess) -----
; NOTE: guard rdotv=0 → log(0) = -inf; mul * shin = -inf; exp = 0. 符合直覺。
log  r0.z,  r0.yyyy
mul  r0.z,  r0.zzzz, c4.xxxx
exp  r0.z,  r0.zzzz           ; spec

; ----- color = base * ndotl + spec_color * spec -----
mul  r7,    c2, r0.xxxx       ; base * ndotl
mad  r7,    c3, r0.zzzz, r7   ; + spec_color * spec

; ----- output rgba (a = 1.0) -----
mov  o0.xyz, r7
mov  o0.w,   c5.yyyy          ; c5.y = 1.0
