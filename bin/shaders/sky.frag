#version 330 core

in vec2 v_ndc;

uniform mat4 u_invViewProj;
uniform vec3 u_cameraPos;

uniform vec3 u_skyColor;
uniform vec3 u_fogColor;
uniform int u_fogEnabled;

out vec4 FragColor;

void main() {
    vec4 clip = vec4(v_ndc, 1.0, 1.0);
    vec4 wpos = u_invViewProj * clip;
    wpos.xyz /= wpos.w;

    vec3 dir = normalize(wpos.xyz - u_cameraPos);

    vec3 fogCol = mix(u_fogColor, vec3(1.0), 0.78);

    float up = clamp(dir.y * 0.5 + 0.5, 0.0, 1.0);

    float skyMix = smoothstep(0.05, 0.65, up);
    vec3 col = mix(fogCol, u_skyColor, skyMix);

    float h = abs(dir.y);
    float ring = exp(- (h * h) / (0.40 * 0.40));
    col = mix(col, fogCol, ring * 0.75);

    FragColor = vec4(col, 1.0);
}
