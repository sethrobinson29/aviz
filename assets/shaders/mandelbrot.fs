#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

out vec4 finalColor;

uniform vec2 resolution;
uniform float zoom;
uniform vec2 juliaC;
uniform float hueShift;
uniform float pulse;
uniform int maxIterations;

vec3 hsv2rgb(vec3 c)
{
    vec4 k = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + k.xyz) * 6.0 - k.www);
    return c.z * mix(k.xxx, clamp(p - k.xxx, 0.0, 1.0), c.y);
}

void main()
{
    vec2 uv = fragTexCoord - vec2(0.5);
    float aspect = resolution.x / max(resolution.y, 1.0);
    uv.x *= aspect;
    uv *= 2.5 / max(zoom, 0.0001);

    vec2 z = uv;
    vec2 c = juliaC;

    int iterations = 0;
    float smoothIterations = 0.0;
    bool escaped = false;

    for (int i = 0; i < 1024; i++)
    {
        if (i >= maxIterations) break;

        float x = z.x * z.x - z.y * z.y + c.x;
        float y = 2.0 * z.x * z.y + c.y;
        z = vec2(x, y);
        iterations = i + 1;

        if (dot(z, z) > 4.0)
        {
            escaped = true;
            break;
        }
    }

    if (escaped)
    {
        float logZn = log(dot(z, z)) * 0.5;
        float nu = log(logZn / log(2.0)) / log(2.0);
        smoothIterations = float(iterations) + 1.0 - nu;
    }

    float t = clamp(smoothIterations / float(maxIterations), 0.0, 1.0);
    float hue = fract(hueShift + t * 0.85);
    float saturation = mix(0.65, 1.0, clamp(pulse, 0.0, 1.0));
    float value = escaped ? mix(0.4, 1.0, sqrt(t)) : 0.0;

    vec3 rgb = hsv2rgb(vec3(hue, saturation, value));
    float alpha = escaped ? 1.0 : 0.0;

    finalColor = vec4(rgb, alpha) * fragColor;
}
