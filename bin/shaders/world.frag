#version 330 core

in vec2 v_uv;
in vec3 v_light;
in vec3 v_tint;
in float v_tintMode;
in float v_worldY;

uniform sampler2D u_texture;
uniform sampler2D u_grassOverlayTexture;
uniform int u_grassSideOverlayEnabled;

out vec4 FragColor;

void main() {
    vec4 tex = texture(u_texture, v_uv);
    if (tex.a < 0.1) discard;

    vec3 color = tex.rgb * v_light;

    if (v_tintMode > 0.5 && u_grassSideOverlayEnabled != 0) {
        vec4 overlay = texture(u_grassOverlayTexture, v_uv);
        float mask = clamp(overlay.a, 0.0, 1.0);
        vec3 foliage = overlay.rgb * v_tint * v_light;
        color = mix(color, foliage, mask);
    }

    FragColor = vec4(color, tex.a);
}
