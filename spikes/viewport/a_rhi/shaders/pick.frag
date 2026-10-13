#version 440
// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later


layout(location = 0) in vec3 vNormal;
layout(location = 1) in float vFace;
layout(location = 2) in vec3 vWorld;
layout(location = 3) in vec4 vLight;

layout(location = 0) out vec4 fragColor;

void main() {
    uint id = uint(vFace + 0.5);
    fragColor = vec4(float(id & 255u) / 255.0, float((id >> 8) & 255u) / 255.0,
                     float((id >> 16) & 255u) / 255.0, 1.0);
}
