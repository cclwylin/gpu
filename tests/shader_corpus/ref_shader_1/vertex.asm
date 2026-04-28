; ref_shader_1 VS — Gouraud + texture coord pass-through
; ISA v1.0
;
; ABI:
;   r0 = a_pos     (attribute slot 0)
;   r1 = a_color   (attribute slot 1)
;   r2 = a_uv      (attribute slot 2, .xy populated, .zw = 0)
;   c0..c3 = u_mvp (row-major)
;   c4     = u_tint
;   o0 = gl_Position
;   o1 = v_color
;   o2 = v_uv

; gl_Position = u_mvp * a_pos
dp4  o0.x,  c0,  r0
dp4  o0.y,  c1,  r0
dp4  o0.z,  c2,  r0
dp4  o0.w,  c3,  r0

; v_color = a_color * u_tint
mul  o1,    r1,  c4

; v_uv = a_uv
mov  o2,    r2
