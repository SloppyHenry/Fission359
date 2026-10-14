#version 440
// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// One triangle that covers the whole image; no vertex buffer.
layout(location = 0) out vec2 vUv;

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
} u;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    vec2 p = vec2(float((gl_VertexIndex << 1) & 2), float(gl_VertexIndex & 2));
    // vUv: (0, 0) top left, (1, 1) bottom right, independent of the graphics API
    vUv = vec2(p.x, 1.0 - p.y);
    gl_Position = u.corr * vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
