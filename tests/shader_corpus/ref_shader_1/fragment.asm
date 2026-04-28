; ref_shader_1 FS — sample texture * Gouraud color
; ISA v1.0
;
; ABI:
;   v0 = v_color
;   v1 = v_uv (.xy)
;   tex0 = u_tex
;   o0 = gl_FragColor

tex  r0,  v1.xy, tex0       ; r0 = texture2D(u_tex, v_uv)
mul  o0,  r0,    v0         ; gl_FragColor = texel * v_color
