#version 330 core

layout(location = 0) in vec4 vertex;
layout(location = 1) in vec4 color;
layout(location = 2) in vec3 normal;

uniform mat4 P;
uniform mat4 V;
uniform mat4 M;

out vec3 fragPos;
out vec3 fragNormal;
out vec4 fragColor;

void main() {
    gl_Position = P * V * M * vertex;
    fragPos = vec3(M * vertex);
    fragNormal = normalize(mat3(transpose(inverse(M))) * normal); // interpolowane poprawnie
    fragColor = color;
}
