#version 440
// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Pick pass: every vertex carries the number of the face or edge it belongs to.
layout(location = 0) in vec3 position;
layout(location = 1) in uint id;

layout(location = 0) flat out uint vId;

layout(std140, binding = 0) uniform Params {
    mat4 mvp;  // clip-space correction * crop * projection * view
    vec4 misc; // x, y: size of one pixel in clip space of the pick target
} u;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    vId = id;
    gl_Position = u.mvp * vec4(position, 1.0);
}
