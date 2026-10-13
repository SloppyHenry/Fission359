#version 440
// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

layout(location = 0) in vec3 vNormal;
layout(location = 1) in float vFace;
layout(location = 2) in vec3 vWorld;
layout(location = 3) in vec4 vLight;

layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform Params {
    mat4 mvp;
    mat4 lightVp;
    vec4 ids;
    vec4 color;
    vec4 eye;
    vec4 lightDir;
    vec4 flags;
} u;

layout(binding = 1) uniform sampler2D shadowMap;

float shadowAmount(vec3 normal) {
    vec3 ndc = vLight.xyz / vLight.w;
    vec2 uv = ndc.xy * 0.5 + 0.5;
    if (u.flags.x > 0.5) {
        uv.y = 1.0 - uv.y;
    }
    float depth = ndc.z * 0.5 + 0.5;
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0 || depth > 1.0) {
        return 1.0;
    }
    float bias = max(0.0015 * (1.0 - dot(normal, u.lightDir.xyz)), 0.0006);
    vec2 texel = vec2(u.flags.y);
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
        n = -n; // two-sided: faces are lit from the viewer's side
    }
    float id = floor(vFace + 0.5);
    bool isBed = vFace < -0.5;
    if (isBed) {
        n = vec3(0.0, 0.0, 1.0);
    }

    float shadow = shadowAmount(n);
    float diffuse = max(dot(n, u.lightDir.xyz), 0.0) * shadow;
    float fill = max(dot(n, viewDir), 0.0);
    float ambient = 0.30 + 0.12 * n.z;

    vec3 base = u.color.rgb;
    if (isBed) {
        base = vec3(0.19, 0.20, 0.23);
        // grid: thin lines every 10 mm, stronger every 50 mm (constant width in world units)
        vec2 g = abs(fract(vWorld.xy / 10.0 - 0.5) - 0.5) * 10.0;
        float line = (min(g.x, g.y) < 0.12) ? 1.0 : 0.0;
        vec2 g5 = abs(fract(vWorld.xy / 50.0 - 0.5) - 0.5) * 50.0;
        float strong = (min(g5.x, g5.y) < 0.25) ? 1.0 : 0.0;
        base += vec3(0.05) * line + vec3(0.07) * strong;
    } else if (abs(id - u.ids.y) < 0.5) {
        base = vec3(1.0, 0.62, 0.15); // selected
    }
    vec3 c = base * (ambient + 0.62 * diffuse + 0.14 * fill);
    if (!isBed && abs(id - u.ids.x) < 0.5) {
        c = mix(c, vec3(0.35, 0.75, 1.0), 0.55); // hovered
    }
    fragColor = vec4(c, 1.0);
}
