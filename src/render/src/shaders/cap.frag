#version 440
// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The cut face of the section view: the colour of the body, a little darker, with diagonal
// stripes laid out in world space (so they keep still when the camera moves).

layout(location = 0) in vec4 vColor;
layout(location = 1) in vec3 vWorld;

layout(location = 0) out vec4 fragColor;

void main() {
    float phase = fract((vWorld.x + vWorld.y + vWorld.z) / 3.0);
    float stripe = step(0.75, phase); // a quarter of the width: a thin dark line
    vec3 base = vColor.rgb * 0.82;
    vec3 c = mix(base, base * 0.55, stripe);
    fragColor = vec4(c, 1.0);
}
