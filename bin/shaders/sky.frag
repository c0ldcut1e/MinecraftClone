#version 330 core

in vec2 v_ndc;

uniform mat4 u_invViewProj;
uniform vec3 u_cameraPos;

uniform sampler2D u_skyColormap;
uniform sampler2D u_fogColormap;

uniform int u_fogEnabled;

// fog lookup stays stable
uniform vec2 u_fogLut;

// sky lookup controls:
// x = maxX (how far right we allow sampling)
// y = yTop (preferred row near top)
// z = yHorizon (preferred row near horizon)
uniform vec3 u_skyLut;

out vec4 FragColor;

vec3 sample_colormap(sampler2D map, float x, float y) {
    return texture(map, vec2(clamp(x, 0.0, 1.0), clamp(y, 0.0, 1.0))).rgb;
}

void main() {
    vec4 clip = vec4(v_ndc, 1.0, 1.0);
    vec4 wpos = u_invViewProj * clip;
    wpos.xyz /= wpos.w;

    vec3 dir = normalize(wpos.xyz - u_cameraPos);

    // 0..1 where 1 is straight up
    float up = clamp(dir.y * 0.5 + 0.5, 0.0, 1.0);
    up = clamp(up, 0.002, 0.998);

    // Base sky sampling
    float skyMaxX = clamp(u_skyLut.x, 0.0, 1.0);
    float x = mix(0.0, skyMaxX, up);

    float y = mix(u_skyLut.z, u_skyLut.y, up);

    // IMPORTANT: sky.png is a TRIANGLE LUT:
    // valid region is bottom-left triangle where x + y <= 1.
    // Force y under the diagonal so we never sample the white top-right triangle.
    float eps = 0.002;
    y = min(y, 1.0 - x - eps);
    y = max(y, 0.0);

    vec3 skyCol = sample_colormap(u_skyColormap, x, y);
    vec3 fogCol = sample_colormap(u_fogColormap, u_fogLut.x, u_fogLut.y);

    // Fog haze toward horizon
    float horizon = 1.0 - up;
    float haze = smoothstep(0.10, 0.70, horizon);
    if (u_fogEnabled == 0) haze = 0.0;

    vec3 col = mix(skyCol, fogCol, haze);
    FragColor = vec4(col, 1.0);
}
