#version 440
// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later


layout(location = 0) in vec3 position;

layout(std140, binding = 0) uniform Params {
    mat4 mvp;
    mat4 lightVp;
    vec4 ids;
    vec4 color;
    vec4 eye;
    vec4 lightDir;
    vec4 flags;
} u;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    gl_Position = u.mvp * vec4(position, 1.0);
}
