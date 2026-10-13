#version 440
// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in float faceId;

layout(location = 0) out vec3 vNormal;
layout(location = 1) out float vFace;
layout(location = 2) out vec3 vWorld;
layout(location = 3) out vec4 vLight;

layout(std140, binding = 0) uniform Params {
    mat4 mvp;
    mat4 lightVp;   // light view-projection, OpenGL conventions (no backend correction)
    vec4 ids;       // x: hovered face, y: selected face
    vec4 color;     // base color
    vec4 eye;       // camera position, world space
    vec4 lightDir;  // direction towards the light, world space
    vec4 flags;     // x: 1 if texture y runs downwards (Metal, Vulkan, D3D)
} u;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    vNormal = normal;
    vFace = faceId;
    vWorld = position;
    vLight = u.lightVp * vec4(position, 1.0);
    gl_Position = u.mvp * vec4(position, 1.0);
}
