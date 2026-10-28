#version 440
// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// One triangle that covers the whole target. vNdc is the position in normalised device
// coordinates (y up), the same on every graphics API.
layout(location = 0) out vec2 vNdc;

layout(std140, binding = 0) uniform Ao {
    mat4 invViewProj;
    mat4 corr;        // clip-space correction of the backend
    vec4 eye;
    vec4 a;
    vec4 b;
} ao;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    vec2 p = vec2(float((gl_VertexIndex << 1) & 2), float(gl_VertexIndex & 2));
    vNdc = p * 2.0 - 1.0;
    gl_Position = ao.corr * vec4(vNdc, 0.0, 1.0);
}
