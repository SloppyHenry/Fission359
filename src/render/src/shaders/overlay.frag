#version 440
// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

layout(location = 0) in vec3 vWorld;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform Params {
    mat4 mvp;
    vec4 color;
    vec4 clip;
} u;

void main() {
    if (dot(u.clip.xyz, vWorld) > u.clip.w) {
        discard;
    }
    fragColor = u.color;
}
