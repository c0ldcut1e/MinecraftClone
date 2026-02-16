#version 330 core

in vec2 v_uv;

out vec4 o_color;

uniform sampler2D u_capture;

uniform float u_alpha;
uniform float u_thickness;
uniform float u_gap;
uniform float u_arm;

void main() {
    vec2 p = v_uv * 2.0 - 1.0;

    float ax = abs(p.x);
    float ay = abs(p.y);

    float inV = step(ax, u_thickness) * step(ay, u_arm);
    float inH = step(ay, u_thickness) * step(ax, u_arm);

    float inside = max(inV, inH);

    if (u_gap > 0.0) {
        float cut = step(ax, u_gap) * step(ay, u_gap);
        inside = inside * (1.0 - cut);
    }

    if (inside < 0.5) discard;

    vec4 src = texture(u_capture, vec2(v_uv.x, 1.0 - v_uv.y));
    vec3 inv = vec3(1.0) - src.rgb;

    o_color = vec4(inv, u_alpha);
}
