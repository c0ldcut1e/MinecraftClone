#version 330 core

in vec2 v_uv;
in vec3 v_light;
in float v_fogSpherical;
in float v_fogCylindrical;
in float v_worldY;

uniform sampler2D u_texture;

uniform vec3 u_fogColor;
uniform vec2 u_fogRange;
uniform int u_fogEnabled;
uniform vec3 u_cameraPos;

out vec4 FragColor;

float linear_fog_value(float d, float s, float e) {
    s /= 3.0;
    if (d <= s) return 0.0;
    if (d >= e) return 1.0;
    return (d - s) / (e - s);
}

void main() {
    vec4 tex = texture(u_texture, v_uv);
    vec3 rgb = tex.rgb * v_light;

    if (u_fogEnabled != 0) {
        float fogStart = u_fogRange.x;
        float fogEnd   = u_fogRange.y;

        float a = linear_fog_value(v_fogSpherical, fogStart, fogEnd);
        float b = linear_fog_value(v_fogCylindrical, fogStart, fogEnd);
        float fogValue = max(a, b);

        fogValue = smoothstep(0.0, 1.0, fogValue);
        fogValue = pow(fogValue, 2.4);

        vec3 fogCol = mix(u_fogColor, vec3(1.0), 0.65);

        rgb = mix(rgb, fogCol, fogValue);

        float camLow = clamp((32.0 - u_cameraPos.y) / 32.0, 0.0, 1.0);
        float bottom = clamp((32.0 - v_worldY) / 32.0, 0.0, 1.0);
        bottom = bottom * bottom;
        float bottomAmount = bottom * fogValue * 0.65 * camLow;

        rgb = mix(rgb, vec3(0.0), bottomAmount);
    }

    FragColor = vec4(rgb, tex.a);
}
