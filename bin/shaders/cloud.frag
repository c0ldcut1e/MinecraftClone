#version 330 core

in vec4 v_color;

uniform float u_cloudLight;

out vec4 FragColor;

float sat(float x) { return clamp(x, 0.0, 1.0); }

void main() {
    float l = sat(u_cloudLight);

    float rgbMul = mix(0.10, 1.0, l);

    float night = 1.0 - l;
    vec3 rgb = v_color.rgb * rgbMul;
    rgb *= mix(0.65, 1.0, l);

    float a = v_color.a;

    FragColor = vec4(rgb, a);
}
