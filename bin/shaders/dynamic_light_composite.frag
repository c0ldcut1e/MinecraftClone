#version 460 core

in vec2 v_uv;

out vec4 fragColor;

uniform sampler2D u_sceneColorTexture;
uniform sampler2D u_albedoTexture;
uniform sampler2D u_lightColorTexture;
uniform float u_strength;

const float REFLECTIVITY = 0.05;

void main()
{
    vec4 sceneColor = texture(u_sceneColorTexture, v_uv);
    vec4 albedo = texture(u_albedoTexture, v_uv);
    vec3 lightColor = texture(u_lightColorTexture, v_uv).rgb * max(u_strength, 0.0);
    float surfaceMask = clamp(albedo.a, 0.0, 1.0);
    vec3 litSurface = albedo.rgb * lightColor * (1.0 - REFLECTIVITY);
    vec3 reflectedLight = lightColor * REFLECTIVITY;
    vec3 finalColor = sceneColor.rgb + (litSurface + reflectedLight) * surfaceMask;
    fragColor = vec4(finalColor, sceneColor.a);
}
