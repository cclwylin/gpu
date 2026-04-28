attribute vec4 a_pos;
attribute vec3 a_normal;

uniform mat4 u_mvp;
uniform mat4 u_model;

varying vec3 v_wpos;
varying vec3 v_normal;

void main() {
    gl_Position = u_mvp * a_pos;
    v_wpos      = (u_model * a_pos).xyz;
    v_normal    = mat3(u_model) * a_normal;
}
