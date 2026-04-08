#version 460 core

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec2 a_uv;
layout (location = 3) in vec4 a_tintMode;
layout (location = 6) in vec4 a_atlasRect;
layout (location = 7) in vec3 a_normal;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_projection;
uniform vec3 u_chunkOffset;

out vec2 v_uv;
out vec3 v_tint;
out float v_tintMode;
out vec4 v_atlasRect;
out vec3 v_normal;
out vec3 v_worldPosition;

void main()
{
    vec3 localPosition = a_position * vec3(16.0, 256.0, 16.0) + u_chunkOffset;
    gl_Position = u_projection * u_view * u_model * vec4(localPosition, 1.0);
    v_uv = a_uv;
    v_tint = a_tintMode.rgb;
    v_tintMode = a_tintMode.a;
    v_atlasRect = a_atlasRect;
    v_normal = a_normal;
    v_worldPosition = localPosition;
}
