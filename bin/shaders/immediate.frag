#version 330 core

in vec2 v_uv;
in vec4 v_color;

uniform sampler2D u_texture;
uniform int u_useTexture;

out vec4 FragColor;

void main() {
    vec4 color = v_color;

    if (u_useTexture == 1) color *= texture(u_texture, v_uv);

    FragColor = color;
}
