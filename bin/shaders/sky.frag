#version 330 core

in vec2 v_ndc;

uniform mat4 u_invProj;
uniform mat4 u_invViewRot;

uniform sampler2D u_skyColormap;
uniform sampler2D u_fogColormap;
uniform int u_fogEnabled;
uniform vec2 u_fogLut;
uniform vec3 u_skyLut;

uniform float u_celestialAngle;
uniform vec3 u_sunDir;

uniform float u_dayFactor;

out vec4 FragColor;

float sat(float x) { return clamp(x, 0.0, 1.0); }

vec3 sample0(sampler2D map, vec2 uv) {
    return textureLod(map, clamp(uv, 0.0, 1.0), 0.0).rgb;
}

void main() {
    vec4 v = u_invProj * vec4(v_ndc, 1.0, 1.0);
    vec3 dirView = normalize(v.xyz / max(v.w, 1e-6));
    vec3 dir = normalize(mat3(u_invViewRot) * dirView);

    float up01 = sat(dir.y * 0.5 + 0.5);
    float horizon01 = 1.0 - up01;

    float ang = u_celestialAngle * 6.28318530718;
    float angCos = cos(ang);

    float dayFactor = sat(u_dayFactor);
    float night = 1.0 - dayFactor;

    float xSafe = 0.06;
    vec3 dayLow  = sample0(u_skyColormap, vec2(xSafe, 0.18));
    vec3 dayHigh = sample0(u_skyColormap, vec2(xSafe, 0.88));
    float dayGrad = pow(smoothstep(0.0, 1.0, up01), 0.85);
    vec3 daySky = mix(dayLow, dayHigh, dayGrad);

    vec3 nightHorizon = vec3(0.02, 0.02, 0.04);
    vec3 nightZenith  = vec3(0.0, 0.0, 0.0);
    float nightGrad = pow(sat(up01), 1.65);
    vec3 nightSky = mix(nightHorizon, nightZenith, nightGrad);

    vec3 sky = mix(nightSky, daySky, dayFactor);

    vec3 fogCol = sample0(u_fogColormap, u_fogLut);

    float haze = smoothstep(0.0, 0.75, horizon01);
    haze = pow(haze, 0.4);
    if (u_fogEnabled == 0) haze = 0.0;

    float hazeScale = dayFactor * 0.95 + 0.05;
    haze *= hazeScale;

    vec3 col = mix(sky, fogCol, haze);

    float edge = sat(1.0 - up01);
    float sunFacing = sat(dot(normalize(u_sunDir), dir));
    float duskWindow = 1.0 - abs(angCos) / 0.4;
    duskWindow = sat(duskWindow);

    float duskShape = duskWindow * duskWindow;
    float duskMask = pow(edge, 2.6) * pow(sunFacing, 2.0);
    float dusk = duskShape * duskMask;

    vec3 duskCol = vec3(0.80, 0.25, 0.05);
    col = mix(col, duskCol, dusk);

    float below01 = sat(-dir.y);
    float voidBlend = smoothstep(0.12, 0.20, below01);
    float deep = smoothstep(0.55, 1.0, below01);

    vec3 dayVoidShallow = vec3(0.39, 0.43, 0.78);
    vec3 dayVoidDeep    = vec3(0.02, 0.02, 0.08);
    vec3 dayVoidCol = mix(dayVoidShallow, dayVoidDeep, deep);

    vec3 nightVoidShallow = vec3(0.01, 0.01, 0.02);
    vec3 nightVoidDeep    = vec3(0.00, 0.00, 0.00);
    vec3 nightVoidCol = mix(nightVoidShallow, nightVoidDeep, deep);

    vec3 voidCol = mix(nightVoidCol, dayVoidCol, dayFactor);

    float voidStrength = mix(0.35, 1.0, dayFactor);

    col = mix(col, voidCol, voidBlend * voidStrength);

    FragColor = vec4(col, 1.0);
}
