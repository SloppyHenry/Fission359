#version 440
// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later


layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in float faceId;

layout(location = 0) out vec3 vNormal;
layout(location = 1) out float vFace;

layout(std140, binding = 0) uniform Params {
    mat4 mvp;
    mat4 viewRotation;
    vec4 ids;   // x: hovered face, y: selected face
    vec4 color; // base color
} u;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    vNormal = mat3(u.viewRotation) * normal;
    vFace = faceId;
    gl_Position = u.mvp * vec4(position, 1.0);
}
