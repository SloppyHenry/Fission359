#version 440
// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Ambient occlusion from the depth of the scene: how much of the surroundings of a point lies
// above its surface and close by. 1 = open, smaller = in a corner or a crevice.
layout(location = 0) in vec2 vNdc;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform Ao {
    mat4 invViewProj;  // world from normalised device coordinates (OpenGL depth range)
    mat4 corr;
    vec4 eye;          // xyz: camera position in the world
    vec4 a;            // x: 1 if texture y runs downwards, y: radius in mm,
                       // z: pixels per mm at distance 1 (perspective), w: pixels per mm (orthographic, else 0)
    vec4 b;            // x, y: size of the depth image in pixels, z: strength
} ao;

layout(binding = 1) uniform sampler2D depthMap;
layout(binding = 2) uniform sampler2D normalMap;

const int kSamples = 12;

float depthAt(vec2 ndc) {
    vec2 uv = ndc * 0.5 + 0.5;
    if (ao.a.x > 0.5) {
        uv.y = 1.0 - uv.y;
    }
    return texture(depthMap, uv).r;
}

vec3 worldAt(vec2 ndc, float depth) {
    vec4 p = ao.invViewProj * vec4(ndc, depth * 2.0 - 1.0, 1.0);
    return p.xyz / p.w;
}

void main() {
    float depth = depthAt(vNdc);
    if (depth >= 0.99999) {
        fragColor = vec4(1.0); // background
        return;
    }
    vec3 p = worldAt(vNdc, depth);

    // The surface normal as written by the prepass (the side the viewer sees).
    vec2 px = 2.0 / ao.b.xy;
    vec2 uv = vNdc * 0.5 + 0.5;
    if (ao.a.x > 0.5) {
        uv.y = 1.0 - uv.y;
    }
    vec3 n = normalize(texture(normalMap, uv).xyz * 2.0 - 1.0);

    float ppm = ao.a.w > 0.0 ? ao.a.w : ao.a.z / max(length(ao.eye.xyz - p), 1e-3);
    float radiusPx = clamp(ao.a.y * ppm, 3.0, 48.0);
    float radius = radiusPx / ppm; // in mm: what the clamp left of it

    // a different rotation for each pixel; the blur afterwards takes it out again
    float noise = fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));
    float occlusion = 0.0;
    for (int i = 0; i < kSamples; ++i) {
        float angle = (float(i) + noise) * 2.39996323; // golden angle
        float distance = radiusPx * sqrt((float(i) + 0.5) / float(kSamples));
        vec2 offset = vec2(cos(angle), sin(angle)) * distance * px;
        vec2 ndc = vNdc + offset;
        vec3 s = worldAt(ndc, depthAt(ndc));
        vec3 v = s - p;
        float length2 = dot(v, v);
        float lengthV = sqrt(length2 + 1e-6);
        float above = max(dot(n, v) / lengthV - 0.1, 0.0); // flat surfaces stay clean
        float near = 1.0 - smoothstep(0.6 * radius, radius, lengthV);
        occlusion += above * near;
    }
    float open = 1.0 - ao.b.z * clamp(occlusion / float(kSamples) * 3.0, 0.0, 1.0);
    fragColor = vec4(open, open, open, 1.0);
}
