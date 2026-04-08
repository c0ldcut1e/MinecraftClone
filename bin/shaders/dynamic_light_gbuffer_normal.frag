#version 460 core

in vec2 v_uv;
in vec4 v_atlasRect;
in vec3 v_normal;

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

vec3 safeNormalize(vec3 value)
{
    float lengthSquared = dot(value, value);
    if (lengthSquared <= 0.000001)
    {
        return vec3(0.0, 1.0, 0.0);
    }
    return value * inversesqrt(lengthSquared);
}

void main()
{
    vec4 tex = texture(u_texture, atlasUv(v_uv));
    float reveal = smoothstep(0.0, 1.0, clamp(u_chunkAlpha, 0.0, 1.0));
    if (tex.a < 0.1 || reveal <= 0.0)
    {
        discard;
    }

    vec3 normal = safeNormalize(v_normal);
    fragColor = vec4(normal * 0.5 + 0.5, tex.a * reveal);
}
