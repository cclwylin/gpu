// Gouraud VS — transform + pass-through color + uv.
attribute vec4 a_pos;       // attribute slot 0
attribute vec4 a_color;     // attribute slot 1
attribute vec2 a_uv;        // attribute slot 2

uniform mat4 u_mvp;         // c0..c3
uniform vec4 u_tint;        // c4

varying vec4 v_color;       // o1
varying vec2 v_uv;          // o2

void main() {
    gl_Position = u_mvp * a_pos;
    v_color     = a_color * u_tint;
    v_uv        = a_uv;
}
