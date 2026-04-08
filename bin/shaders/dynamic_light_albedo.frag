#version 460 core

in vec2 v_uv;
in vec3 v_tint;
in float v_tintMode;
in vec4 v_atlasRect;

out vec4 fragColor;

uniform sampler2D u_texture;
uniform sampler2D u_grassOverlayTexture;
uniform int u_grassSideOverlayEnabled;
uniform float u_chunkAlpha;
uniform vec3 u_chunkFadeColor;

vec2 atlasUv(vec2 uv)
{
    vec2 wrapped = fract(uv);
    if (wrapped.x == 0.0 && uv.x > 0.0)
    {
        wrapped.x = 1.0;
    }
    if (wrapped.y == 0.0 && uv.y > 0.0)
    {
        wrapped.y = 1.0;
    }
    return mix(v_atlasRect.xy, v_atlasRect.zw, wrapped);
}

void main()
{
    vec4 tex = texture(u_texture, atlasUv(v_uv));
    if (tex.a < 0.1)
    {
        discard;
    }

    vec3 color = tex.rgb;
    if (v_tintMode <= 0.5)
    {
        color *= v_tint;
    }

    if (v_tintMode > 0.5 && u_grassSideOverlayEnabled != 0)
    {
        vec4 overlay = texture(u_grassOverlayTexture, v_uv);
        float mask = clamp(overlay.a, 0.0, 1.0);
        vec3 foliage = overlay.rgb * v_tint;
        color = mix(color, foliage, mask);
    }

    float reveal = smoothstep(0.0, 1.0, clamp(u_chunkAlpha, 0.0, 1.0));
    vec3 fadeColor = mix(u_chunkFadeColor, vec3(1.0), 0.12);
    color = mix(fadeColor, color, reveal);
    fragColor = vec4(color, tex.a * reveal);
}
