#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform float uVignetteStrength;
uniform float uBloomThreshold;
uniform float uBloomStrength;

out vec4 finalColor;

float luma(vec3 c)
{
    return dot(c, vec3(0.2126, 0.7152, 0.0722));
}

vec3 sample_scene(vec2 uv)
{
    return texture(texture0, uv).rgb;
}

float cyan_glow_mask(vec3 c)
{
    const vec3 target = vec3(95.0, 205.0, 228.0) / 255.0; // #5fcde4
    float dist = length(c - target);
    // Keep this relatively tolerant because particles blend with white background.
    return 1.0 - smoothstep(0.10, 0.44, dist);
}

void main()
{
    vec2 uv = fragTexCoord;
    vec2 texel = 1.0 / vec2(textureSize(texture0, 0));

    vec3 scene = sample_scene(uv);

    vec3 glow = scene * cyan_glow_mask(scene) * 0.6;

    vec2 offsets[24] = vec2[](
        vec2( 1.0,  0.0), vec2(-1.0,  0.0), vec2( 0.0,  1.0), vec2( 0.0, -1.0),
        vec2( 1.0,  1.0), vec2(-1.0,  1.0), vec2( 1.0, -1.0), vec2(-1.0, -1.0),
        vec2( 2.0,  0.0), vec2(-2.0,  0.0), vec2( 0.0,  2.0), vec2( 0.0, -2.0),
        vec2( 2.0,  1.0), vec2(-2.0,  1.0), vec2( 2.0, -1.0), vec2(-2.0, -1.0),
        vec2( 1.0,  2.0), vec2(-1.0,  2.0), vec2( 1.0, -2.0), vec2(-1.0, -2.0),
        vec2( 2.0,  2.0), vec2(-2.0,  2.0), vec2( 2.0, -2.0), vec2(-2.0, -2.0)
    );
    float radii[3] = float[](1.5, 3.0, 5.0);
    float radius_weight[3] = float[](1.0, 0.7, 0.4);

    for (int r = 0; r < 3; r++) {
        for (int i = 0; i < 24; i++) {
            vec3 s = sample_scene(uv + offsets[i] * texel * radii[r]);
            float bright = max(luma(s) - uBloomThreshold, 0.0);
            float cyan = cyan_glow_mask(s);
            float w = max(bright * 0.35, cyan) * radius_weight[r];
            glow += s * w;
        }
    }
    glow *= (1.0 / 24.0) * 0.9;

    vec3 color = scene + glow * uBloomStrength;

    vec2 centered = uv * 2.0 - 1.0;
    float edge = length(centered);
    float vignette = 1.0 - smoothstep(0.35, 1.15, edge);
    color *= mix(1.0, vignette, uVignetteStrength);

    finalColor = vec4(color, 1.0) * fragColor * colDiffuse;
}
