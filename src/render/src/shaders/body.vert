#version 440
// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec4 color;

layout(location = 0) out vec3 vNormal;
layout(location = 1) out vec4 vColor;
layout(location = 2) out vec3 vWorld;
layout(location = 3) out vec4 vLight;

layout(std140, binding = 0) uniform Params {
    mat4 mvp;
    mat4 lightVp;    // light view-projection, OpenGL conventions (no backend correction)
    mat4 corr;       // clip-space correction of the backend
    vec4 eye;        // camera position, world space
    vec4 lightDir;   // direction towards the light, world space
    vec4 bedColor;
    vec4 gridColor;
    vec4 gridMajor;
    vec4 bgCenter;
    vec4 bgEdge;
    vec4 shadowColor; // rgb, a = strength
    vec4 params;     // x: 1 if texture y runs downwards, y: 1 / shadow map size, z: grid mm, w: major mm
    vec4 bedSize;    // x, y: half size, z: shadow map valid (1/0)
    vec4 bodyOverride; // rgb, a = 1: draw every body in this colour
    vec4 view;       // x, y: size of the image in pixels, z: 1 if ambient occlusion is on
} u;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    vNormal = normal;
    vColor = color;
    vWorld = position;
    vLight = u.lightVp * vec4(position, 1.0);
    gl_Position = u.mvp * vec4(position, 1.0);
}
