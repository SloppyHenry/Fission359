#version 440
// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Pick pass for corners: a corner is a small square of fixed size in pixels around its point.
layout(location = 0) in vec3 position;
layout(location = 1) in uint id;
layout(location = 2) in vec2 offset; // in pixels, from the corner point

layout(location = 0) flat out uint vId;
layout(location = 1) out vec3 vWorld;

layout(std140, binding = 0) uniform Params {
    mat4 mvp;
    vec4 misc; // x, y: size of one pixel in clip space of the pick target
    vec4 clip; // see body.vert
} u;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    vId = id;
    vWorld = position;
    vec4 p = u.mvp * vec4(position, 1.0);
    p.xy += offset * u.misc.xy * p.w;
    gl_Position = p;
}
