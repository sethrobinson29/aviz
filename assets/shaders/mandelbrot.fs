#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

out vec4 finalColor;

uniform vec2 resolution;
uniform float zoom;
uniform vec2 juliaC;
uniform float hueShift;
uniform int maxIterations;
uniform float boundaryMarginPx;

vec3 hsv2rgb(vec3 c)
{
    vec4 k = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + k.xyz) * 6.0 - k.www);
    return c.z * mix(k.xxx, clamp(p - k.xxx, 0.0, 1.0), c.y);
}

vec2 complexMul(vec2 a, vec2 b)
{
    return vec2(a.x * b.x - a.y * b.y, a.x * b.y + a.y * b.x);
}

void main()
{
    vec2 uv = fragTexCoord - vec2(0.5);
    float aspect = resolution.x / max(resolution.y, 1.0);
    uv.x *= aspect;
    uv *= 2.5 / max(zoom, 0.0001);

    vec2 z = uv;
    vec2 c = juliaC;
    vec2 dz = vec2(1.0, 0.0);

    int iterations = 0;
    bool escaped = false;

    for (int i = 0; i < 1024; i++)
    {
        if (i >= maxIterations) break;

        dz = 2.0 * complexMul(z, dz);
        z = complexMul(z, z) + c;
        iterations = i + 1;

        if (dot(z, z) > 4.0)
        {
            escaped = true;
            break;
        }
    }

    float t = 0.0;
    float pixelDistance = 0.0;

    if (escaped)
    {
        float logZn = log(dot(z, z)) * 0.5;
        float nu = log(logZn / log(2.0)) / log(2.0);
        float smoothIterations = float(iterations) + 1.0 - nu;
        t = clamp(smoothIterations / float(maxIterations), 0.0, 1.0);

        float zLen = length(z);
        float dzLen = max(length(dz), 0.0001);
        float distanceEstimate = 0.5 * zLen * log(zLen) / dzLen;
        float worldPerPixel = (2.5 / max(zoom, 0.0001)) / max(resolution.y, 1.0);
        pixelDistance = distanceEstimate / max(worldPerPixel, 0.0000001);
    }

    float hue = fract(hueShift + t * 0.85);
    float value = escaped ? mix(0.4, 1.0, sqrt(t)) : 1.0;
    vec3 rgb = hsv2rgb(vec3(hue, 1.0, value));

    float margin = max(boundaryMarginPx, 0.0001);
    float boundaryFade = clamp(1.0 - pixelDistance / margin, 0.0, 1.0);
    float alpha = escaped ? boundaryFade : 1.0;

    finalColor = vec4(rgb, alpha) * fragColor;
}
