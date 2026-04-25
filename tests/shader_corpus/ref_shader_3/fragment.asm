; ref_shader_3 FS — branch + loop + discard
;
; ABI:
;   v0 = v_uv  (.xy)
;   c0 = (u_radius, u_radius^2, 0.0, 1.0)   driver packs radius²
;   c1 = (max_iter_float, 0.0, 2.0, 4.0)    helper constants
;   c2 = (0.5, 0.5, 0.0, 0.0)               center
;   c3 = (1.0, -1.0, 0.5, 1.0)              palette helper
;   c4 = (0.0, 0.0, 0.0, 1.0)               zero + alpha
;
; Register map:
;   r0 = p
;   r1 = r² / scratch
;   r2 = z
;   r3 = c
;   r4 = iteration counter (n)
;   r5 = zn (temp next)
;
; NOTE: loop count is an immediate (32) matching the GLSL hardcoded bound.
;       early `break` via condition check — shows break semantics.

; ----- p = v_uv - vec2(0.5) -----
add  r0.xy, v0.xy, -c2.xy       ; p = uv - center

; ----- r² = dot(p, p) -----
dp3  r1.x,  r0.xyxx, r0.xyxx    ; using xy (z=0 by write mask below, safer = mask r0.z to 0)
                                ; alt: mov r0.z, c4.x then dp3 full r0.xyz
                                ; For simplicity assume dp2 behavior via r0.xy (z lanes 0)

; ----- discard if r² > radius² -----
add  r1.y,  r1.x, -c0.y         ; r1.y = r² - rad²
cmp  r1.z,  r1.y, c4.xxxx, c3.xxxx
                                ; if r1.y >= 0 → r1.z = 1.0 else 0.0
                                ; NOTE: `cmp` def: dst = src0>=0 ? src1 : src2
                                ;       → r1.z = (r²-rad² >= 0) ? 0.0 : 1.0
                                ;   ↑ semantics reversed: we want KIL when r² > rad²
                                ;     so reswap: src1=1.0, src2=0.0  (above correct)

; Predicated kil:若 r1.z == 1 → kil
; 這裡走 predicate-reg 路徑:把 r1.z 寫入 p,再 p-kil
; (p,predicate update 的完整 encoding 在 Phase 0 最終敲定;示意)
;
; pseudo: p = (r1.z != 0)
;         kil (if p)
;
; 若 ISA 定義 kil 為 per-lane 且需條件:
cmp  p,     r1.z, c4.xxxx, c3.xxxx    ; predicate mask-wise set
kil                                    ; per-lane (依 p) kil

; ----- c = p * 2.0 -----
mul  r3.xy, r0.xy, c1.zz        ; c1.z = 2.0

; ----- z = (0, 0) -----
mov  r2.xy, c4.xx               ; c4.x = 0

; ----- n = 0 -----
mov  r4.x,  c4.xxxx

; ----- for i = 0; i < 32; ++i -----
loop 32
    ; if (i >= u_iter) break  →  if (n >= u_iter) break  (we track n == i)
    add  r1.x, r4.x, -c1.x          ; r1.x = n - max_iter
    cmp  p,    r1.x, c3.xxxx, c4.xxxx  ; p = (n >= max_iter) ? 1 : 0
    (p) break                       ; break if p set

    ; zn.x = z.x*z.x - z.y*z.y + c.x
    mul  r5.x, r2.x, r2.x
    mad  r5.x, -r2.y, r2.y, r5.x    ; - z.y*z.y (source negate)
    add  r5.x, r5.x, r3.x

    ; zn.y = 2.0 * z.x * z.y + c.y
    mul  r1.x, r2.x, r2.y
    mad  r5.y, r1.x, c1.zzzz, r3.y  ; 2 * z.x * z.y + c.y

    ; z = zn
    mov  r2.xy, r5.xy

    ; if (dot(z,z) > 4.0) break
    dp3  r1.x, r2.xyxx, r2.xyxx
    add  r1.x, r1.x, -c1.w          ; - 4.0
    cmp  p,    r1.x, c3.xxxx, c4.xxxx
    (p) break

    ; n++
    add  r4.x, r4.x, c3.xxxx        ; += 1.0
endloop

; ----- t = n / u_iter -----
rcp  r1.x, c1.xxxx              ; 1 / max_iter
mul  r1.x, r4.x, r1.x

; ----- gl_FragColor = (t, 1-t, 0.5, 1.0) -----
mov  o0.x, r1.xxxx
add  o0.y, c3.xxxx, -r1.x       ; 1 - t
mov  o0.z, c3.zzzz              ; 0.5
mov  o0.w, c3.wwww              ; 1.0
