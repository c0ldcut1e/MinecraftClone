#version 330 core

in vec2 v_uv;

uniform sampler2D u_colorTexture;
uniform float u_gammaExponent;

out vec4 FragColor;

void main()
{
    vec4 color = texture(u_colorTexture, v_uv);
    vec3 rgb = pow(max(color.rgb, vec3(0.0)), vec3(max(u_gammaExponent, 0.0001)));
    FragColor = vec4(clamp(rgb, 0.0, 1.0), color.a);
}
