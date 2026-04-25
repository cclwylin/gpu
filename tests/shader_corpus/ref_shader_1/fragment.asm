; ref_shader_1 FS — sample tex and modulate with Gouraud color
;
; ABI mapping:
;   v0 = v_color  (from VS o1, interpolated per-sample)
;   v1 = v_uv     (from VS o2, zw undefined)
;   texture binding slot 0 = u_tex
;   o0 = gl_FragColor

; ----- sample texture -----
tex  r0,  v1.xy, tex0       ; r0 = texture2D(u_tex, v_uv)

; ----- multiply by varying color -----
mul  o0,  r0,    v0         ; gl_FragColor = texel * v_color
