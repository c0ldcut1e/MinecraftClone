#version 460 core

in vec2 v_uv;

out vec4 fragColor;

uniform sampler2D u_albedoTexture;
uniform sampler2D u_positionTexture;
uniform sampler2D u_normalTexture;
uniform sampler2D u_lightUvTexture;
uniform sampler2D u_lightColorTexture;

void main()
{
    vec4 albedo = texture(u_albedoTexture, v_uv);
    vec3 position = texture(u_positionTexture, v_uv).xyz;
    vec3 normal = texture(u_normalTexture, v_uv).xyz;
    float lightUv = texture(u_lightUvTexture, v_uv).r;
    vec3 lightColor = texture(u_lightColorTexture, v_uv).rgb;

    vec3 debugColor = mix(albedo.rgb, normal, 0.45);
    debugColor += clamp(lightColor, 0.0, 4.0) * 0.30;
    debugColor = mix(debugColor, vec3(clamp(lightUv * 0.2, 0.0, 1.0)), 0.15);
    debugColor = mix(debugColor, fract(abs(position) * 0.02), 0.12);

    fragColor = vec4(debugColor, 1.0);
}
