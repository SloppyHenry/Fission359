#version 440
// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Takes the per-pixel rotation of the ambient occlusion out: a 4 x 4 box blur in four taps.
layout(location = 0) in vec2 vNdc;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform Ao {
    mat4 invViewProj;
    mat4 corr;
    vec4 eye;
    vec4 a;            // x: 1 if texture y runs downwards
    vec4 b;            // x, y: size of this image in pixels
} ao;

layout(binding = 1) uniform sampler2D rawMap;

void main() {
    vec2 uv = vNdc * 0.5 + 0.5;
    if (ao.a.x > 0.5) {
        uv.y = 1.0 - uv.y;
    }
    vec2 texel = 1.0 / ao.b.xy;
    float sum = 0.0;
    for (int x = -1; x <= 1; x += 2) {
        for (int y = -1; y <= 1; y += 2) {
            sum += texture(rawMap, uv + vec2(float(x), float(y)) * texel).r;
        }
    }
    float open = sum * 0.25;
    fragColor = vec4(open, open, open, 1.0);
}
