#version 330 core

in vec2 v_uv;

uniform sampler2D u_vignette;
uniform float u_strength;

out vec4 FragColor;

void main() {
    vec4 v = texture(u_vignette, v_uv);
    v.rgb = vec3(1.0) - v.rgb;
    FragColor = mix(vec4(1.0), v, clamp(u_strength, 0.0, 1.0));
}
