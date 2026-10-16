#version 440
// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform Params {
    mat4 mvp;
    vec4 color;
} u;

void main() {
    fragColor = u.color;
}
