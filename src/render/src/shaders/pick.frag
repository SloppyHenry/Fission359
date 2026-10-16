#version 440
// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

layout(location = 0) flat in uint vId;
layout(location = 0) out vec4 fragColor;

// The 32 bit number goes into the four 8 bit channels of the picture, unchanged.
void main() {
    fragColor = vec4(float(vId & 255u), float((vId >> 8) & 255u), float((vId >> 16) & 255u),
                     float((vId >> 24) & 255u)) / 255.0;
}
