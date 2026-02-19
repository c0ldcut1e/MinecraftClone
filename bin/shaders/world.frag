#version 330 core

in vec2 v_uv;
in vec3 v_light;
in float v_fogSpherical;
in float v_fogCylindrical;
in float v_worldY;

uniform sampler2D u_texture;

// Colormap for fog
uniform sampler2D u_fogColormap;

// Fog params
uniform vec2 u_fogRange;
uniform int  u_fogEnabled;

// Where to sample the fog colormap (stable!)
uniform vec2 u_fogLut;

out vec4 FragColor;

float linear_fog_value(float d, float s, float e) {
    // keep your original scaling
    s /= 3.0;
    if (d <= s) return 0.0;
    if (d >= e) return 1.0;
    return (d - s) / (e - s);
}

vec3 sample_colormap(sampler2D map, float x, float y) {
    return texture(map, vec2(clamp(x, 0.0, 1.0), clamp(y, 0.0, 1.0))).rgb;
}

void main() {
    vec4 tex = texture(u_texture, v_uv);
    vec3 rgb = tex.rgb * v_light;

    if (u_fogEnabled != 0) {
        float fogStart = u_fogRange.x;
        float fogEnd   = u_fogRange.y;

        float a = linear_fog_value(v_fogSpherical,  fogStart, fogEnd);
        float b = linear_fog_value(v_fogCylindrical, fogStart, fogEnd);
        float fogValue = max(a, b);

        // Minecraft beta-ish: keep it mostly linear, tiny curve optional
        fogValue = clamp(fogValue, 0.0, 1.0);
        fogValue = fogValue * fogValue; // optional. remove if you want pure linear.

        // IMPORTANT: fog color does NOT depend on distance
        vec3 fogCol = sample_colormap(u_fogColormap, u_fogLut.x, u_fogLut.y);

        rgb = mix(rgb, fogCol, fogValue);
    }

    FragColor = vec4(rgb, tex.a);
}
