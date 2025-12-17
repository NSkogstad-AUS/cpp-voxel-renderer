#version 330 core

in vec4 vColor;
in vec3 vNormal;
in vec4 vFragPosLightSpace;
in vec3 vWorldPos;

out vec4 FragColor;

uniform vec3 lightDir;
uniform sampler2D shadowMap;
uniform vec2 shadowTexelSize;

void main() {
    vec3 normal = normalize(vNormal);
    vec3 lightDirN = normalize(-lightDir);
    float diff = max(dot(normal, lightDirN), 0.0);

    // Transform to shadow map space
    vec3 projCoords = vFragPosLightSpace.xyz / vFragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    float shadow = 0.0;
    if (projCoords.z <= 1.0 && projCoords.x >= 0.0 && projCoords.x <= 1.0 && projCoords.y >= 0.0 && projCoords.y <= 1.0) {
        // Smaller, normal-based bias to reduce light leaks/gaps
        float bias = max(0.0005, 0.0025 * (1.0 - diff));
        float currentDepth = projCoords.z - bias;
        float shadowSum = 0.0;
        int samples = 0;
        // 5x5 PCF kernel for cleaner edges
        for (int x = -2; x <= 2; ++x) {
            for (int y = -2; y <= 2; ++y) {
                vec2 offset = vec2(x, y) * shadowTexelSize;
                float closest = texture(shadowMap, projCoords.xy + offset).r;
                shadowSum += currentDepth > closest ? 1.0 : 0.0;
                samples++;
            }
        }
        shadow = shadowSum / float(samples);
    }

    float ambient = 0.25;
    float lighting = ambient + (1.0 - shadow) * diff;
    vec3 color = vColor.rgb * lighting;
    FragColor = vec4(color, vColor.a);
}
