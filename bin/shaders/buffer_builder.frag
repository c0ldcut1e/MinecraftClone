#version 330 core

in vec2 v_uv;
in vec4 v_color;

uniform sampler2D u_texture;
uniform int u_useTexture;
uniform float u_alphaCutoff;

out vec4 FragColor;

void main() {
    vec4 color = v_color;

    if (u_useTexture == 1) {
        vec4 tex = texture(u_texture, v_uv);
        if (tex.a <= u_alphaCutoff) discard;
        color *= tex;
    }

    FragColor = color;
}
