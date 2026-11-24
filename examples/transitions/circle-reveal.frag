#version 300 es

// Circle reveal expanding from a random point, with an ease-in-out curve.

precision highp float;

in vec2 v_texcoord;

uniform sampler2D tex1;     // outgoing frame
uniform sampler2D tex2;     // incoming frame
uniform float     progress; // linear 0..1
uniform float     alpha;
uniform vec2      fullSize; // frame size in pixels
uniform vec2      seed;     // random point in 0..1, new per transition

layout(location = 0) out vec4 fragColor;

const float EDGE = 0.02; // soft edge width, relative to the radius

void main() {
    float eased = smoothstep(0.0, 1.0, progress);

    vec2  center = seed * fullSize;
    float dist   = distance(v_texcoord * fullSize, center);

    // the farthest corner from the center: the radius that covers the whole frame
    float maxDist = length(max(center, fullSize - center));

    // map eased 0..1 to a radius range that starts fully hidden and ends fully revealed
    float radius = mix(-EDGE, 1.0 + EDGE, eased);
    float mask   = smoothstep(radius - EDGE, radius + EDGE, dist / maxDist);

    fragColor = mix(texture(tex2, v_texcoord), texture(tex1, v_texcoord), mask) * alpha;
}
