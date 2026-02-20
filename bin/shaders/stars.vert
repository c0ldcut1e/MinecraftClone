#version 330 core

layout (location = 0) in vec3 a_position;

uniform mat4 u_viewRot;
uniform mat4 u_projection;
uniform float u_starAngle;

void main() {
    float c = cos(u_starAngle);
    float s = sin(u_starAngle);

    vec3 p = vec3(a_position.x * c - a_position.z * s, a_position.y, a_position.x * s + a_position.z * c);

    gl_Position = u_projection * u_viewRot * vec4(p, 1.0);
}
