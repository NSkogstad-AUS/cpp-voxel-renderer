#version 330 core

in vec4 vColor;
in vec3 vNormal;
in vec4 vFragPosLightSpace;
in vec3 vWorldPos;

out vec4 FragColor;

uniform vec3 lightDir;
uniform sampler2D shadowMap;

float calculateShadow(vec4 fragPosLightSpace, float bias) {
    // Transform to [0,1] range
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0) {
        return 0.0;
    }
    float closestDepth = texture(shadowMap, projCoords.xy).r;
    float currentDepth = projCoords.z;
    return currentDepth - bias > closestDepth ? 1.0 : 0.0;
}

void main() {
    vec3 normal = normalize(vNormal);
    vec3 lightDirN = normalize(-lightDir);
    float diff = max(dot(normal, lightDirN), 0.0);
    float bias = max(0.001, 0.005 * (1.0 - diff));
    float shadow = calculateShadow(vFragPosLightSpace, bias);

    float ambient = 0.2;
    float lighting = ambient + (1.0 - shadow) * diff;
    vec3 color = vColor.rgb * lighting;
    FragColor = vec4(color, vColor.a);
}
