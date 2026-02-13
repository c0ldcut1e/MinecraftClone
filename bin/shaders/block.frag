#version 330 core

in vec2 v_uv;
in vec3 v_light;

uniform sampler2D u_texture;

out vec4 FragColor;

void main() {
    vec4 tex = texture(u_texture, v_uv);

    // THIS is the important part
    vec3 lit = tex.rgb * v_light;

    FragColor = vec4(lit, tex.a);
}
