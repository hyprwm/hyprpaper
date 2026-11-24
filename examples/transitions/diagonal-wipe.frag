#version 300 es

// Diagonal wipe: the incoming frame grows from the top-left corner as progress rises.

precision highp float;

in vec2 v_texcoord;

uniform sampler2D tex1;     // outgoing frame
uniform sampler2D tex2;     // incoming frame
uniform float     progress; // linear 0..1
uniform float     alpha;

layout(location = 0) out vec4 fragColor;

const float EDGE = 0.05; // soft edge width

void main() {
    float diagonal = (v_texcoord.x + v_texcoord.y) * 0.5;

    // map progress 0..1 to an edge range that starts fully hidden and ends fully revealed
    float edge = mix(-EDGE, 1.0 + EDGE, progress);
    float mask = smoothstep(edge - EDGE, edge + EDGE, diagonal);

    fragColor = mix(texture(tex2, v_texcoord), texture(tex1, v_texcoord), mask) * alpha;
}
