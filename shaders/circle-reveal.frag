#version 300 es

precision highp float;

in vec2 v_texcoord;

uniform sampler2D tex1;      // Start frame (fit pre-resolved in C++)
uniform sampler2D tex2;      // End frame (fit pre-resolved in C++)
uniform float progress;      // 0.0 = fully start, 1.0 = fully end
uniform float alpha;

uniform vec2 fullSize;       // Output size in pixels
uniform vec2 randomPixel;    // Circle center (0-1 normalized)
uniform float u_duration;    // Host transition duration in seconds (from hyprland config)

// Shader-defined visual duration override: the circle effect completes in 0.5s
// regardless of the host's transition duration. The host's u_duration is used
// to remap progress so the effect fits within this window.
const float SHADER_DURATION = 0.5;

layout(location = 0) out vec4 fragColor;

void main() {
    // Remap progress so the effect completes in SHADER_DURATION seconds.
    float p = (u_duration > 0.0)
        ? clamp(progress * (u_duration / SHADER_DURATION), 0.0, 1.0)
        : progress;

    vec4 startColor = texture(tex1, v_texcoord);
    vec4 endColor   = texture(tex2, v_texcoord);

    // Circle reveal: a radial mask expanding from randomPixel.
    vec2 pixelPos    = v_texcoord * fullSize;
    vec2 centerPixel = randomPixel * fullSize;

    float dist = distance(pixelPos, centerPixel);

    // Farthest corner from the center, in pixel space (aspect-ratio correct).
    float maxDist = 0.0;
    maxDist = max(maxDist, distance(vec2(0.0, 0.0), centerPixel));
    maxDist = max(maxDist, distance(vec2(fullSize.x, 0.0), centerPixel));
    maxDist = max(maxDist, distance(vec2(0.0, fullSize.y), centerPixel));
    maxDist = max(maxDist, distance(fullSize, centerPixel));

    float normalizedDist = dist / maxDist;

    // circleMask: 0 = show start, 1 = show end.
    float edgeWidth   = 0.02;
    float circleMask  = smoothstep(p - edgeWidth, p + edgeWidth, normalizedDist);

    vec4 blended = mix(endColor, startColor, circleMask);

    fragColor = blended * alpha;
}
