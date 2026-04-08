#version 460 core

in vec2 v_uv;
in vec4 v_atlasRect;
in vec3 v_worldPosition;

out vec4 fragColor;

uniform sampler2D u_texture;
uniform float u_chunkAlpha;

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
    float reveal = smoothstep(0.0, 1.0, clamp(u_chunkAlpha, 0.0, 1.0));
    if (tex.a < 0.1 || reveal <= 0.0)
    {
        discard;
    }

    fragColor = vec4(v_worldPosition, tex.a * reveal);
}
