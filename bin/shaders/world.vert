#version 330 core

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec2 a_uv;
layout (location = 2) in vec3 a_light;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_projection;
uniform vec3 u_cameraPos;

out vec2 v_uv;
out vec3 v_light;
out float v_fogSpherical;
out float v_fogCylindrical;
out float v_worldY;

void main() {
    vec4 viewPos = u_view * u_model * vec4(a_position, 1.0);
    gl_Position = u_projection * viewPos;

    v_uv = a_uv;
    v_light = a_light;

    vec3 p = viewPos.xyz;
    v_fogSpherical = length(p);

    float distXZ = length(p.xz);
    float distY = abs(p.y);
    v_fogCylindrical = max(distXZ, distY);

    v_worldY = p.y + u_cameraPos.y;
}
