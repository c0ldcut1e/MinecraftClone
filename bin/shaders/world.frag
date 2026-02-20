#version 330 core

in vec2 v_uv;
in vec3 v_light;
in float v_worldY;

uniform sampler2D u_texture;

out vec4 FragColor;

void main() {
    vec4 tex = texture(u_texture, v_uv);
    FragColor = vec4(tex.rgb * v_light, tex.a);
}
