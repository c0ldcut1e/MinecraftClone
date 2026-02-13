#version 330 core
in vec2 v_uv;

out vec4 FragColor;

uniform sampler2D u_font;
uniform vec3 u_color;
uniform float u_alpha;

void main()
{
    float alpha = texture(u_font, v_uv).r;
    if (alpha < 0.01) discard;
    FragColor = vec4(u_color, alpha * u_alpha);
}
