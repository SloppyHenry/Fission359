#version 440
// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later


layout(location = 0) in vec3 position;

layout(std140, binding = 0) uniform Params {
    mat4 mvp;
    mat4 viewRotation;
    vec4 ids;
    vec4 color;
} u;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    gl_Position = u.mvp * vec4(position, 1.0);
}
