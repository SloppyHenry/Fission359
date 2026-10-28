#version 440
// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Highlights: the faces of the body in one colour, drawn over the body.
layout(location = 0) in vec3 position;

layout(std140, binding = 0) uniform Params {
    mat4 mvp;
    vec4 color;
} u;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    gl_Position = u.mvp * vec4(position, 1.0);
}
