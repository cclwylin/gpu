; ref_shader_3 FS — circle discard + iterative loop (Mandelbrot-ish)
; ISA v1.0  (predicate, loop/break, kil, dp2, setp_*)
;
; ABI:
;   v0 = v_uv (.xy)
;   c0 = (radius, radius^2, 0, 1)
;   c1 = (max_iter_float, 0.0, 2.0, 4.0)
;   c2 = (0.5, 0.5, 0, 0)
;   c3 = (1.0, -1.0, 0.5, 1.0)
;   c4 = (0, 0, 0, 1)
;   o0 = gl_FragColor
;
; Register layout:
;   r0   = p (uv - center)
;   r1   = scratch
;   r2   = z (iterating complex value, .xy)
;   r3   = c (initial coord scaled)
;   r4.x = iteration count n
;   r5   = zn (next z)

; ---- p = v_uv - center ----
add  r0.xy, v0.xy, -c2.xy

; ---- r2 = dot(p, p) ----
dp2  r1.x,  r0.xy, r0.xy

; ---- if (r² > radius²) discard ----
setp_gt  p, r1.x, c0.yyyy
(p) kil

; ---- c = p * 2 ----
mul  r3.xy, r0.xy, c1.zzzz       ; c1.z = 2.0

; ---- z = (0,0) ----
mov  r2.xy, c4.xxxx              ; xx -> 0,0

; ---- n = 0 ----
mov  r4.x,  c4.xxxx

; ---- main loop (32 iterations max) ----
loop 32
    ; if (n >= max_iter) break
    setp_ge p, r4.x, c1.xxxx
    (p) break

    ; zn.x = z.x*z.x - z.y*z.y + c.x
    mul  r5.x, r2.xxxx, r2.xxxx
    mad  r5.x, -r2.yyyy, r2.yyyy, r5.xxxx     ; - z.y*z.y
    add  r5.x, r5.xxxx, r3.xxxx

    ; zn.y = 2*z.x*z.y + c.y
    mul  r1.x, r2.xxxx, r2.yyyy
    mad  r5.y, r1.xxxx, c1.zzzz,  r3.yyyy     ; * 2 + c.y

    ; z = zn
    mov  r2.xy, r5.xy

    ; if (dot(z,z) > 4) break
    dp2  r1.x, r2.xy, r2.xy
    setp_gt p, r1.x, c1.wwww
    (p) break

    ; n += 1
    add  r4.x, r4.xxxx, c3.xxxx               ; c3.x = 1.0
endloop

; ---- t = n / max_iter ----
rcp  r1.x, c1.xxxx
mul  r1.x, r4.xxxx, r1.xxxx

; ---- gl_FragColor = (t, 1-t, 0.5, 1.0) ----
mov  o0.x, r1.xxxx
add  o0.y, c3.xxxx, -r1.xxxx       ; 1 - t
mov  o0.z, c3.zzzz                  ; 0.5
mov  o0.w, c3.wwww                  ; 1.0
