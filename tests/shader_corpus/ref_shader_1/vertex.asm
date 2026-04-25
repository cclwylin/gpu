; ref_shader_1 VS — Gouraud + texture coord pass
;
; ABI mapping (driver-provided):
;   r0 = a_pos     (attribute slot 0, loaded before entry by VF/SC)
;   r1 = a_color   (attribute slot 1)
;   r2 = a_uv.xy00 (attribute slot 2, zero-pad to vec4)
;   c0..c3 = u_mvp (row-major)
;   c4     = u_tint
;   o0 = gl_Position
;   o1 = v_color
;   o2 = v_uv
;
; Convention: dst.writemask, src.swizzle

; ----- gl_Position = u_mvp * a_pos -----
dp4  o0.x, c0, r0           ; row 0 · pos
dp4  o0.y, c1, r0           ; row 1 · pos
dp4  o0.z, c2, r0           ; row 2 · pos
dp4  o0.w, c3, r0           ; row 3 · pos

; ----- v_color = a_color * u_tint -----
mul  o1,   r1,  c4

; ----- v_uv = a_uv -----
mov  o2,   r2

; (SC implicit "done" on end of program)
