; ref_shader_2 VS — transform + pass world pos & normal to FS
;
; ABI:
;   r0 = a_pos       (vec4)
;   r1 = a_normal    (vec3, w undef)
;   c0..c3 = u_mvp
;   c4..c7 = u_model (note: mat3(u_model) = top-left 3x3 = c4.xyz / c5.xyz / c6.xyz)
;   o0 = gl_Position
;   o1 = v_wpos   (xyz)
;   o2 = v_normal (xyz)

; ----- gl_Position = u_mvp * a_pos -----
dp4  o0.x, c0, r0
dp4  o0.y, c1, r0
dp4  o0.z, c2, r0
dp4  o0.w, c3, r0

; ----- v_wpos = (u_model * a_pos).xyz -----
dp4  o1.x, c4, r0
dp4  o1.y, c5, r0
dp4  o1.z, c6, r0
; o1.w unused

; ----- v_normal = mat3(u_model) * a_normal -----
dp3  o2.x, c4, r1
dp3  o2.y, c5, r1
dp3  o2.z, c6, r1
; o2.w unused
