#version 330 core

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec2 a_uv;
layout (location = 2) in vec3 a_light;
layout (location = 3) in vec4 a_tintMode;
layout (location = 4) in vec4 a_rawLight;
layout (location = 5) in float a_shade;
layout (location = 6) in vec4 a_atlasRect;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_projection;
uniform vec3 u_cameraPos;
uniform vec3 u_chunkOffset;
uniform int u_lightingMode;
uniform float u_skyLightLevels[16];

out vec2 v_uv;
out vec3 v_light;
out vec3 v_tint;
out float v_tintMode;
out float v_worldY;
out vec4 v_atlasRect;

void main() {
    vec3 localPosition = a_position * vec3(16.0, 256.0, 16.0) + u_chunkOffset;
    vec4 viewPos = u_view * u_model * vec4(localPosition, 1.0);
    gl_Position  = u_projection * viewPos;
    v_uv         = a_uv;
    if (u_lightingMode == 1) {
        vec4 rawLight = a_rawLight * 15.0;
        int skyIndex = clamp(int(rawLight.w + 0.5), 0, 15);
        float sky = u_skyLightLevels[skyIndex];
        vec3 base = max(rawLight.rgb / 15.0, vec3(sky));
        base = max(base, vec3(0.03));
        vec3 light = base * a_shade;
        if (a_tintMode.a <= 0.5) {
            light *= a_tintMode.rgb;
        }
        v_light = light;
    } else if (u_lightingMode == 2) {
        v_light = vec3(1.0);
    } else {
        v_light = a_light;
    }
    v_tint       = a_tintMode.rgb;
    v_tintMode   = a_tintMode.a;
    v_worldY     = viewPos.y + u_cameraPos.y;
    v_atlasRect  = a_atlasRect;
}
