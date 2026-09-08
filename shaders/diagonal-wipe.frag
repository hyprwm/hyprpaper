#version 300 es

precision highp float;

in vec2 v_texcoord;

uniform sampler2D tex1;      // Start frame (fit pre-resolved in C++)
uniform sampler2D tex2;      // End frame (fit pre-resolved in C++)
uniform float progress;      // 0.0 = fully start, 1.0 = fully end
uniform float alpha;

layout(location = 0) out vec4 fragColor;

void main() {
    vec4 startColor = texture(tex1, v_texcoord);
    vec4 endColor   = texture(tex2, v_texcoord);

    // Diagonal wipe expanding from the top-left corner as progress increases.
    float diagonal  = (v_texcoord.x + v_texcoord.y) * 0.5;
    float edgeWidth = 0.05;

    // wipeMask: 0 = show end, 1 = show start.
    float wipeMask = smoothstep(progress - edgeWidth, progress + edgeWidth, diagonal);

    vec4 blended = mix(endColor, startColor, wipeMask);

    fragColor = blended * alpha;
}
