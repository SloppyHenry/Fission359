#version 440
// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

layout(location = 0) in vec3 vWorld;
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
    vec4 bodyOverride;
    vec4 view;
    vec4 clip; // see body.vert
} u;

void main() {
    if (dot(u.clip.xyz, vWorld) > u.clip.w) {
        discard;
    }
    fragColor = vec4(0.0);
}
