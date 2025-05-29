#version 330 core

layout(location = 0) in vec3 vertex;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoord;

uniform mat4 M;
uniform mat4 V;
uniform mat4 P;
uniform mat3 normalMatrix;

out vec3 fragPos;
out vec3 fragNormal;
out vec2 fragTexCoord;

void main() {
    vec4 worldPosition = M * vec4(vertex, 1.0);
    fragPos = vec3(worldPosition);
    fragNormal = normalize(normalMatrix * normal);
    fragTexCoord = texCoord;
    gl_Position = P * V * worldPosition;
}
