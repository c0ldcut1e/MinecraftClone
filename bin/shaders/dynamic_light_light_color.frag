#version 460 core

in vec2 v_uv;

out vec4 fragColor;

uniform sampler2D u_positionTexture;
uniform sampler2D u_normalTexture;
uniform int u_lightCount;
uniform float u_ambientLight;
uniform vec4 u_lightPositionRadius[64];
uniform vec4 u_lightColorBrightness[64];
uniform vec4 u_lightDirectionType[64];
uniform vec4 u_lightRightWidth[64];
uniform vec4 u_lightUpHeight[64];
uniform vec4 u_lightAreaAngleDistance[64];

const float FACE_BRIGHTNESS_DOWN = 0.5;
const float FACE_BRIGHTNESS_UP = 1.0;
const float FACE_BRIGHTNESS_NORTH = 0.8;
const float FACE_BRIGHTNESS_SOUTH = 0.8;
const float FACE_BRIGHTNESS_WEST = 0.6;
const float FACE_BRIGHTNESS_EAST = 0.6;

vec3 safeNormalize(vec3 value)
{
    float lengthSquared = dot(value, value);
    if (lengthSquared <= 0.000001)
    {
        return vec3(0.0, 1.0, 0.0);
    }
    return value * inversesqrt(lengthSquared);
}

float attenuateNoCusp(float distanceValue, float radiusValue)
{
    float safeRadius = max(radiusValue, 0.0001);
    float normalized = distanceValue / safeRadius;
    if (normalized >= 1.0)
    {
        return 0.0;
    }

    float oneMinusS = 1.0 - normalized;
    return oneMinusS * oneMinusS * oneMinusS;
}

float pointDiffuse(vec3 normal, vec3 lightDirection)
{
    float diffuse = max(dot(normal, lightDirection), 0.0);
    return (diffuse + u_ambientLight) / (1.0 + u_ambientLight);
}

float areaDiffuse(vec3 normal, vec3 lightDirection)
{
    float diffuse = (dot(normal, lightDirection) + 1.0) * 0.5;
    return (diffuse + u_ambientLight) / (1.0 + u_ambientLight);
}

float directionalDiffuse(vec3 normal, vec3 lightDirection)
{
    return clamp(smoothstep(-0.2, 0.2, -dot(normal, lightDirection)), 0.0, 1.0);
}

float blockFaceBrightness(vec3 normal)
{
    float darkFromDown = pow(clamp(-normal.y, 0.0, 1.0), 3.0) * FACE_BRIGHTNESS_DOWN;
    float darkFromUp = pow(clamp(normal.y, 0.0, 1.0), 3.0) * FACE_BRIGHTNESS_UP;
    float darkFromNorth = pow(clamp(-normal.z, 0.0, 1.0), 2.0) * FACE_BRIGHTNESS_NORTH;
    float darkFromSouth = pow(clamp(normal.z, 0.0, 1.0), 2.0) * FACE_BRIGHTNESS_SOUTH;
    float darkFromWest = pow(clamp(-normal.x, 0.0, 1.0), 2.0) * FACE_BRIGHTNESS_WEST;
    float darkFromEast = pow(clamp(normal.x, 0.0, 1.0), 2.0) * FACE_BRIGHTNESS_EAST;
    return darkFromDown + darkFromUp + darkFromNorth + darkFromSouth + darkFromWest +
           darkFromEast;
}

float liftDiffuse(float diffuse, vec3 normal, float strength)
{
    return max(diffuse, blockFaceBrightness(normal) * strength);
}

float computeConeScale(float coneCos)
{
    float safeCos = clamp(coneCos, 0.0001, 0.999999);
    return sqrt(max(1.0 - safeCos * safeCos, 0.0)) / safeCos;
}

float computeAreaSpotMask(vec3 toSurface, vec3 lightRight, vec3 lightUp, vec3 lightForward,
                          vec2 shape, float outerConeCos)
{
    float forwardDistance = dot(toSurface, lightForward);
    if (forwardDistance <= 0.0001)
    {
        return 0.0;
    }

    float outerScale = max(computeConeScale(outerConeCos) * forwardDistance, 0.0001);
    float innerConeCos = cos(acos(clamp(outerConeCos, -1.0, 1.0)) * 0.7);
    float innerScale = max(computeConeScale(innerConeCos) * forwardDistance, 0.0001);
    vec2 local = vec2(dot(toSurface, lightRight), dot(toSurface, lightUp));
    vec2 normalized = local / (max(shape, vec2(0.01)) * outerScale);
    float metric = length(normalized);
    float innerThreshold = clamp(innerScale / outerScale, 0.0, 1.0);
    float fadeStart = clamp(innerThreshold - (1.0 - innerThreshold) * 0.75, 0.0, 1.0);
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
    vec3 lightColorBuffer = vec3(0.0);

    for (int i = 0; i < u_lightCount; i++)
    {
        vec3 lightColor = max(u_lightColorBrightness[i].xyz, vec3(0.0));
        float brightness = max(u_lightColorBrightness[i].w, 0.0);
        vec3 lightDirection = safeNormalize(u_lightDirectionType[i].xyz);
        int lightType = int(u_lightDirectionType[i].w + 0.5);

        if (lightType == 0)
        {
            float diffuse = liftDiffuse(directionalDiffuse(normal, lightDirection), normal, 0.08);
            if (diffuse > 0.0)
            {
                lightColorBuffer += lightColor * brightness * diffuse;
            }
            continue;
        }

        if (lightType == 1)
        {
            vec3 lightPosition = u_lightPositionRadius[i].xyz;
            float lightRadius = max(u_lightPositionRadius[i].w, 0.0001);
            vec3 offset = lightPosition - worldPosition;
            float distanceToLight = length(offset);
            if (distanceToLight > lightRadius)
            {
                continue;
            }

            vec3 toLight = safeNormalize(offset);
            float diffuse = liftDiffuse(pointDiffuse(normal, toLight), normal, 0.12);
            float attenuation = attenuateNoCusp(distanceToLight, lightRadius);
            lightColorBuffer += lightColor * brightness * diffuse * attenuation;
            continue;
        }

        vec3 lightPosition = u_lightPositionRadius[i].xyz;
        vec3 lightRight = safeNormalize(u_lightRightWidth[i].xyz);
        vec3 lightUp = safeNormalize(u_lightUpHeight[i].xyz);
        vec2 shape = vec2(max(u_lightRightWidth[i].w, 0.01), max(u_lightUpHeight[i].w, 0.01));
        float outerConeCos = clamp(u_lightAreaAngleDistance[i].x, -1.0, 1.0);
        float maxDistance = max(u_lightAreaAngleDistance[i].y, 0.0001);
        vec3 offset = lightPosition - worldPosition;
        float distanceToLight = length(offset);
        if (distanceToLight > maxDistance)
        {
            continue;
        }

        vec3 toSurface = worldPosition - lightPosition;
        vec3 toLight = safeNormalize(offset);
        float diffuse = liftDiffuse(areaDiffuse(normal, toLight), normal, 0.12);
        float attenuation = attenuateNoCusp(distanceToLight, maxDistance);
        float spotMask =
                computeAreaSpotMask(toSurface, lightRight, lightUp, lightDirection, shape,
                                    outerConeCos);
        lightColorBuffer += lightColor * brightness * diffuse * attenuation * spotMask;
    }

    fragColor = vec4(max(lightColorBuffer, vec3(0.0)), 1.0);
}
