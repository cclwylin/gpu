precision mediump float;

varying vec2 v_uv;   // in [0,1]

uniform float u_radius;   // circle radius (e.g. 0.45)
uniform int   u_iter;     // max iterations (compile-time or bounded)

void main() {
    vec2 p = v_uv - vec2(0.5);
    float r2 = dot(p, p);
    if (r2 > u_radius * u_radius) {
        discard;
    }

    // Iterative refinement: z_{n+1} = z_n * z_n + c  (Mandelbrot-style, bounded)
    vec2 z = vec2(0.0);
    vec2 c = p * 2.0;
    int n = 0;
    for (int i = 0; i < 32; ++i) {
        if (i >= u_iter) break;
        vec2 zn;
        zn.x = z.x * z.x - z.y * z.y + c.x;
        zn.y = 2.0 * z.x * z.y       + c.y;
        z = zn;
        if (dot(z, z) > 4.0) {
            break;
        }
        n++;
    }

    float t = float(n) / float(u_iter);
    gl_FragColor = vec4(t, 1.0 - t, 0.5, 1.0);
}
