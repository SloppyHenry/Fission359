#version 440
// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Depth prepass for the ambient occlusion: writes the surface normal (the side the viewer
// sees) as a colour; the depth goes into the depth attachment.
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
    vec4 view;       // x, y: size of the image in pixels, z: 1 if ambient occlusion is on
} u;

void main() {
    vec3 n = normalize(vNormal);
    if (dot(n, u.eye.xyz - vWorld) < 0.0) {
        n = -n;
    }
    fragColor = vec4(n * 0.5 + 0.5, 1.0);
}
