precision mediump float;

varying vec3 v_wpos;
varying vec3 v_normal;

uniform vec3 u_light_pos;
uniform vec3 u_view_pos;
uniform vec3 u_base_color;
uniform vec3 u_spec_color;
uniform float u_shininess;

void main() {
    vec3 N = normalize(v_normal);
    vec3 L = normalize(u_light_pos - v_wpos);
    vec3 V = normalize(u_view_pos  - v_wpos);
    vec3 R = reflect(-L, N);

    float ndotl = max(dot(N, L), 0.0);
    float rdotv = max(dot(R, V), 0.0);
    float spec  = pow(rdotv, u_shininess);

    vec3 color = u_base_color * ndotl + u_spec_color * spec;
    gl_FragColor = vec4(color, 1.0);
}
