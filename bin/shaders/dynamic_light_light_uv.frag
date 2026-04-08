#version 460 core

in vec2 v_uv;

out vec4 fragColor;

uniform sampler2D u_positionTexture;
uniform sampler2D u_normalTexture;
uniform int u_lightCount;
uniform vec4 u_lightPositionRange[64];
uniform vec4 u_lightColorIntensity[64];
uniform vec4 u_lightDirectionRadius[64];
uniform vec4 u_lightRightSoftness[64];
uniform vec4 u_lightUpType[64];
uniform vec4 u_lightExtentsFalloff[64];
uniform vec4 u_lightCone[64];
uniform vec4 u_lightShapeOffsetPower[64];
uniform vec4 u_lightShapeShearEdge[64];

vec3 safeNormalize(vec3 value)
{
    float lengthSquared = dot(value, value);
    if (lengthSquared <= 0.000001)
    {
        return vec3(0.0, 1.0, 0.0);
    }
    return value * inversesqrt(lengthSquared);
}

float computeFalloff(float distanceValue, float rangeValue, float falloff, float softness)
{
    float safeRange = max(rangeValue, 0.0001);
    float normalized = clamp(1.0 - distanceValue / safeRange, 0.0, 1.0);
    float shaped = pow(normalized, max(falloff, 0.0001));
    float smoothValue = smoothstep(0.0, 1.0, normalized);
    return mix(shaped, smoothValue, clamp(softness, 0.0, 1.0));
}

float computeConeScale(float coneCos)
{
    float safeCos = clamp(coneCos, 0.0001, 0.999999);
    return sqrt(max(1.0 - safeCos * safeCos, 0.0)) / safeCos;
}

float computeSpotMetric(vec2 local, float boxiness, float shapePower)
{
    float circularMetric = length(local);
    float squareMetric = max(abs(local.x), abs(local.y));
    float metric = mix(circularMetric, squareMetric, clamp(boxiness, 0.0, 1.0));
    return pow(max(metric, 0.0), max(shapePower, 0.0001));
}

float computeSpotMask(vec3 toSurface, vec3 lightRight, vec3 lightUp, vec3 lightForward,
                      vec3 lightExtents, vec2 shapeOffset, vec2 shapeShear, float shapePower,
                      float edgeSoftness, float innerCone, float outerCone)
{
    float forwardDistance = dot(toSurface, lightForward);
    if (forwardDistance <= 0.0001)
    {
        return 0.0;
    }

    float outerScale = max(computeConeScale(outerCone) * forwardDistance, 0.0001);
    float innerScale = max(computeConeScale(innerCone) * forwardDistance, 0.0001);
    vec2 local = vec2(dot(toSurface, lightRight), dot(toSurface, lightUp));
    local -= shapeOffset * outerScale;
    local += shapeShear * forwardDistance;

    vec2 shape = max(lightExtents.xy, vec2(0.01));
    float metric = computeSpotMetric(local / (shape * outerScale), lightExtents.z, shapePower);
    float innerThreshold = clamp(innerScale / outerScale, 0.0, 1.0);
    float fadeStart = clamp(
            innerThreshold - (1.0 - innerThreshold) * clamp(edgeSoftness, 0.001, 1.0), 0.0, 1.0);
    return 1.0 - smoothstep(fadeStart, 1.0, metric);
}

void main()
{
    vec4 positionSample = texture(u_positionTexture, v_uv);
    vec4 normalSample = texture(u_normalTexture, v_uv);
    if (positionSample.a <= 0.0 || normalSample.a <= 0.0)
    {
        fragColor = vec4(0.0);
        return;
    }

    vec3 worldPosition = positionSample.xyz;
    vec3 normal = safeNormalize(normalSample.xyz * 2.0 - 1.0);
    float lightUv = 0.0;

    for (int i = 0; i < u_lightCount; i++)
    {
        vec3 lightPosition = u_lightPositionRange[i].xyz;
        float lightRange = max(u_lightPositionRange[i].w, 0.0001);
        float lightIntensity = max(u_lightColorIntensity[i].w, 0.0);
        vec3 lightForward = safeNormalize(u_lightDirectionRadius[i].xyz);
        float lightRadius = max(u_lightDirectionRadius[i].w, 0.0);
        vec3 lightRight = safeNormalize(u_lightRightSoftness[i].xyz);
        float lightSoftness = clamp(u_lightRightSoftness[i].w, 0.0, 1.0);
        vec3 lightUp = safeNormalize(u_lightUpType[i].xyz);
        int lightType = int(u_lightUpType[i].w + 0.5);
        vec3 lightExtents = max(u_lightExtentsFalloff[i].xyz, vec3(0.01));
        float lightFalloff = max(u_lightExtentsFalloff[i].w, 0.0001);
        float innerCone = u_lightCone[i].x;
        float outerCone = u_lightCone[i].y;
        vec2 shapeOffset = u_lightShapeOffsetPower[i].xy;
        float shapePower = u_lightShapeOffsetPower[i].z;
        vec2 shapeShear = u_lightShapeShearEdge[i].xy;
        float edgeSoftness = u_lightShapeShearEdge[i].z;

        vec3 toLight = lightPosition - worldPosition;
        float distanceToLight = length(toLight);
        float outerRadius = max(lightRange, lightRadius + 0.0001);
        if (distanceToLight > outerRadius)
        {
            continue;
        }

        vec3 lightDirection = safeNormalize(toLight);
        float diffuse = max(dot(normal, lightDirection), 0.0);
        if (diffuse <= 0.0)
        {
            continue;
        }

        float attenuation = computeFalloff(max(distanceToLight - lightRadius, 0.0),
                                           max(outerRadius - lightRadius, 0.0001), lightFalloff,
                                           lightSoftness);

        if (lightType == 1)
        {
            vec3 toSurface = worldPosition - lightPosition;
            attenuation *= computeSpotMask(toSurface, lightRight, lightUp, lightForward,
                                           lightExtents, shapeOffset, shapeShear, shapePower,
                                           edgeSoftness, innerCone, outerCone);
        }
        if (attenuation <= 0.0)
        {
            continue;
        }

        lightUv += diffuse * attenuation * lightIntensity;
    }

    fragColor = vec4(vec3(max(lightUv, 0.0)), 1.0);
}
