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
out float v_worldY;

void main() {
    vec4 viewPos = u_view * u_model * vec4(a_position, 1.0);
    gl_Position  = u_projection * viewPos;
    v_uv         = a_uv;
    v_light      = a_light;
    v_worldY     = viewPos.y + u_cameraPos.y;
}
