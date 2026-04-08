#version 330 core

in vec2 v_uv;

uniform sampler2D u_currentColor;
uniform sampler2D u_currentDepth;
uniform sampler2D u_historyColor;
uniform sampler2D u_historyDepth;
uniform int u_hasHistory;
uniform float u_historyWeight;
uniform float u_depthTolerance;
uniform float u_depthFadeRange;

out vec4 FragColor;

void main()
{
    vec4 currentColor = texture(u_currentColor, v_uv);
    float currentDepth = texture(u_currentDepth, v_uv).r;
    if (currentDepth >= 0.99999 || currentColor.a <= 0.0001)
    {
        FragColor = vec4(0.0);
        return;
    }

    vec3 color = currentColor.rgb;

    if (u_hasHistory != 0 && u_historyWeight > 0.0001)
    {
        vec4 historyColor = texture(u_historyColor, v_uv);
        float historyDepth = texture(u_historyDepth, v_uv).r;
        if (historyDepth < 0.99999 && historyColor.a > 0.0001)
        {
            float depthDelta = abs(currentDepth - historyDepth);
            float depthMatch = 1.0 - smoothstep(u_depthTolerance, u_depthTolerance + u_depthFadeRange, depthDelta);
            float historyFactor = clamp(u_historyWeight * depthMatch * historyColor.a, 0.0, 0.95);
            color = mix(color, historyColor.rgb, historyFactor);
        }
    }

    FragColor = vec4(color, currentColor.a);
}
