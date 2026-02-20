#version 330 core

uniform float u_starAlpha;

out vec4 FragColor;

float sat(float x) { return clamp(x, 0.0, 1.0); }

void main() {
    float a = sat(u_starAlpha);
    FragColor = vec4(1.0, 1.0, 1.0, a);
}
