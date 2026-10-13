#version 440
// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later


layout(location = 0) in vec3 vNormal;
layout(location = 1) in float vFace;

layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform Params {
    mat4 mvp;
    mat4 viewRotation;
    vec4 ids;
    vec4 color;
} u;

void main() {
    vec3 n = normalize(vNormal);
    if (n.z < 0.0) {
        n = -n; // two-sided: faces are lit from the viewer's side
    }
    const vec3 lightDirection = normalize(vec3(-0.35, 0.55, 0.8));
    float diffuse = max(dot(n, lightDirection), 0.0);
    float rim = pow(1.0 - n.z, 2.0) * 0.12;

    float id = floor(vFace + 0.5);
    vec3 base = u.color.rgb;
    if (abs(id - u.ids.y) < 0.5) {
        base = vec3(1.0, 0.62, 0.15); // selected
    }
    vec3 c = base * (0.30 + 0.70 * diffuse) + rim;
    if (abs(id - u.ids.x) < 0.5) {
        c = mix(c, vec3(0.35, 0.75, 1.0), 0.55); // hovered
    }
    fragColor = vec4(c, 1.0);
}
