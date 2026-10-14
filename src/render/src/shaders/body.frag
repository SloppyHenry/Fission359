#version 440
// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec4 vColor;
layout(location = 2) in vec3 vWorld;
layout(location = 3) in vec4 vLight;

layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform Params {
    mat4 mvp;
    mat4 lightVp;
    mat4 corr;
    vec4 eye;
    vec4 lightDir;
    vec4 bedColor;
    vec4 gridColor;
    vec4 gridMajor;
    vec4 bgCenter;
    vec4 bgEdge;
    vec4 shadowColor;
    vec4 params;
    vec4 bedSize;
    vec4 bodyOverride; // rgb, a = 1: draw every body in this colour
} u;

layout(binding = 1) uniform sampler2D shadowMap;

// 1 = in the light, 0 = in the shadow; a soft edge from 3 x 3 samples.
float lightAmount(vec3 normal) {
    if (u.bedSize.z < 0.5) {
        return 1.0;
    }
    vec3 ndc = vLight.xyz / vLight.w;
    vec2 uv = ndc.xy * 0.5 + 0.5;
    if (u.params.x > 0.5) {
        uv.y = 1.0 - uv.y;
    }
    float depth = ndc.z * 0.5 + 0.5;
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0 || depth > 1.0) {
        return 1.0;
    }
    float bias = max(0.0015 * (1.0 - dot(normal, u.lightDir.xyz)), 0.0006);
    vec2 texel = vec2(u.params.y);
    float lit = 0.0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float stored = texture(shadowMap, uv + vec2(x, y) * texel * 1.5).r;
            lit += (depth - bias <= stored) ? 1.0 : 0.0;
        }
    }
    return lit / 9.0;
}

void main() {
    vec3 viewDir = normalize(u.eye.xyz - vWorld);
    vec3 n = normalize(vNormal);
    if (dot(n, viewDir) < 0.0) {
        n = -n; // two-sided: a face is lit from the side the viewer sees
    }
    float lit = lightAmount(n);
    float diffuse = max(dot(n, u.lightDir.xyz), 0.0) * lit;
    float fill = max(dot(n, viewDir), 0.0);
    float ambient = 0.46 + 0.14 * n.z;
    vec3 base = u.bodyOverride.a > 0.5 ? u.bodyOverride.rgb : vColor.rgb;
    vec3 c = base * (ambient + 0.5 * diffuse + 0.1 * fill);
    fragColor = vec4(c, vColor.a);
}
