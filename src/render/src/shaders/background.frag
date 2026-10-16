#version 440
// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

layout(location = 0) in vec2 vUv;
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
    vec4 view;       // x, y: size of the image in pixels, z: 1 if ambient occlusion is on
} u;

void main() {
    // like the mockup: radial-gradient(ellipse 85% 75% at 50% 45%, view, view-2)
    float t = length(vec2((vUv.x - 0.5) / 0.85, (vUv.y - 0.45) / 0.75));
    fragColor = mix(u.bgCenter, u.bgEdge, clamp(t, 0.0, 1.0));
}
