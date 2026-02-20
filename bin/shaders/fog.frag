#version 330 core

in vec2 v_uv;

uniform sampler2D u_colorTexture;
uniform sampler2D u_depthTexture;
uniform sampler2D u_fogColormap;
uniform vec2 u_fogLut;
uniform vec2 u_fogRange;
uniform int u_fogEnabled;
uniform mat4 u_invViewProj;

uniform float u_fogStrength;

out vec4 FragColor;

float linearFogValue(float d, float start, float end) {
    start /= 3.0;
    if (d <= start) return 0.0;
    if (d >= end)   return 1.0;
    return (d - start) / (end - start);
}

float reconstructViewDistance(float depthSample) {
    float ndcZ = depthSample * 2.0 - 1.0;
    vec2 ndcXY = v_uv * 2.0 - 1.0;
    vec4 clipPos = vec4(ndcXY, ndcZ, 1.0);
    vec4 worldPos = u_invViewProj * clipPos;
    worldPos /= worldPos.w;
    return length(worldPos.xyz);
}

void main()
{
    vec4 sceneColor  = texture(u_colorTexture, v_uv);
    float depthSample = texture(u_depthTexture, v_uv).r;
    if (u_fogEnabled == 0 || depthSample >= 1.0 || u_fogStrength <= 0.0001) {
        FragColor = sceneColor;
        return;
    }

    float viewDist = reconstructViewDistance(depthSample);
    float fogValue = linearFogValue(viewDist, u_fogRange.x, u_fogRange.y);
    fogValue = clamp(fogValue, 0.0, 1.0);
    fogValue = pow(fogValue, 1.5);
    fogValue *= clamp(u_fogStrength, 0.0, 1.0);

    vec3 fogColor = texture(u_fogColormap, clamp(u_fogLut, 0.0, 1.0)).rgb;
    vec3 rgb = mix(sceneColor.rgb, fogColor, fogValue);
    FragColor = vec4(rgb, sceneColor.a);
}
