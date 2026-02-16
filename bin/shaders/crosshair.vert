#version 330 core

layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec2 a_uv;

out vec2 v_uv;

uniform vec2 u_resolution;

void main() {
    v_uv = a_uv;

    vec2 ndc;
    ndc.x = (a_pos.x / u_resolution.x) * 2.0 - 1.0;
    ndc.y = 1.0 - (a_pos.y / u_resolution.y) * 2.0;

    gl_Position = vec4(ndc, a_pos.z, 1.0);
}
