#version 330 core

in vec2 v_ndc;

uniform mat4 u_invProj;        // NEW: inverse(projection)
uniform mat4 u_invViewRot;     // NEW: inverse(view rotation) only, in mat4 form

uniform sampler2D u_skyColormap;
uniform sampler2D u_fogColormap;
uniform int u_fogEnabled;
uniform vec2 u_fogLut;
uniform vec3 u_skyLut;

out vec4 FragColor;

vec3 sample_lod0(sampler2D map, vec2 uv) {
    uv = clamp(uv, 0.0, 1.0);
    return textureLod(map, uv, 0.0).rgb;
}

void main() {
    // Build view-space ray from NDC using inverse projection
    vec4 v = u_invProj * vec4(v_ndc, 1.0, 1.0);
    vec3 dirView = normalize(v.xyz / v.w);

    // Rotate ray into world space using ONLY rotation (no translation)
    vec3 dir = normalize(mat3(u_invViewRot) * dirView);

    float up = clamp(dir.y * 0.5 + 0.5, 0.0, 1.0);

    float xSafe = 0.06;
    float yLow  = 0.18;
    float yHigh = 0.88;
    vec3 skyLow  = sample_lod0(u_skyColormap, vec2(xSafe, yLow));
    vec3 skyHigh = sample_lod0(u_skyColormap, vec2(xSafe, yHigh));
    float t = pow(smoothstep(0.0, 1.0, up), 0.85);
    vec3 skyCol = mix(skyLow, skyHigh, t);

    vec3 fogCol = sample_lod0(u_fogColormap, u_fogLut);
    float horizon = 1.0 - up;
    float haze = smoothstep(0.0, 0.75, horizon);
    haze = pow(haze, 0.4);
    if (u_fogEnabled == 0) haze = 0.0;
    vec3 col = mix(skyCol, fogCol, haze);

    vec3 alphaVoidColor = vec3(0.39, 0.43, 0.78);
    float belowHorizon = clamp(-dir.y, 0.0, 1.0);
    float voidBlend = smoothstep(0.15, 0.20, belowHorizon);
    float deepFade  = smoothstep(0.6, 1.0, belowHorizon);
    vec3 voidCol = mix(alphaVoidColor, vec3(0.02, 0.02, 0.08), deepFade);
    col = mix(col, voidCol, voidBlend);

    FragColor = vec4(col, 1.0);
}
