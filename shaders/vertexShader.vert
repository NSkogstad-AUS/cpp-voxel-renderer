#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aColor;
layout(location = 2) in vec3 aNormal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpaceMatrix;

out vec4 vColor;
out vec3 vNormal;
out vec4 vFragPosLightSpace;
out vec3 vWorldPos;

void main() {
    vec4 worldPos = model * vec4(aPos, 1.0);
    vWorldPos = worldPos.xyz;
    vColor = aColor;
    vNormal = mat3(model) * aNormal;
    vFragPosLightSpace = lightSpaceMatrix * worldPos;
    gl_Position = projection * view * worldPos;
}
