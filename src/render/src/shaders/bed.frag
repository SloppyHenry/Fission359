#version 440
// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec4 vColor;
layout(location = 2) in vec3 vWorld;
layout(location = 3) in vec4 vLight;

layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform Params {
    mat4 mvp;
    mat4 lightVp;
    mat4 corr;
    vec4 eye;
    vec4 lightDir;
    vec4 bedColor;
    vec4 gridColor;
    vec4 gridMajor;
    vec4 bgCenter;
    vec4 bgEdge;
    vec4 shadowColor;
    vec4 params;
    vec4 bedSize;
    vec4 bodyOverride; // rgb, a = 1: draw every body in this colour
} u;

layout(binding = 1) uniform sampler2D shadowMap;

float shadowAmount() {
    if (u.bedSize.z < 0.5) {
        return 0.0;
    }
    vec3 ndc = vLight.xyz / vLight.w;
    vec2 uv = ndc.xy * 0.5 + 0.5;
    if (u.params.x > 0.5) {
        uv.y = 1.0 - uv.y;
    }
    float depth = ndc.z * 0.5 + 0.5;
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0 || depth > 1.0) {
        return 0.0;
    }
    vec2 texel = vec2(u.params.y);
    float dark = 0.0;
    for (int x = -2; x <= 2; ++x) {
        for (int y = -2; y <= 2; ++y) {
            float stored = texture(shadowMap, uv + vec2(x, y) * texel * 1.5).r;
            dark += (depth - 0.0012 > stored) ? 1.0 : 0.0;
        }
    }
    return dark / 25.0;
}

// Coverage (0..1) of a one pixel wide line at every multiple of `step` mm.
float gridLine(vec2 p, float step) {
    vec2 distance = abs(fract(p / step - 0.5) - 0.5) * step;
    vec2 pixel = fwidth(p);
    vec2 coverage = clamp(1.0 - (distance - 0.25 * pixel) / (1.1 * pixel), 0.0, 1.0);
    return max(coverage.x, coverage.y);
}

void main() {
    vec3 color = u.bedColor.rgb;
    if (vWorld.z > -0.001) {
        // top: grid, minor lines every `grid` mm, stronger lines every `major` mm
        float minor = gridLine(vWorld.xy, u.params.z);
        float major = gridLine(vWorld.xy, u.params.w);
        color = mix(color, u.gridColor.rgb, minor);
        color = mix(color, u.gridMajor.rgb, major);
        color = mix(color, u.shadowColor.rgb, u.shadowColor.a * shadowAmount());
    } else {
        // the plate's edge, a little darker than the top
        color *= 0.8 + 0.2 * clamp(dot(normalize(vNormal), u.lightDir.xyz), 0.0, 1.0);
    }
    fragColor = vec4(color, 1.0);
}
