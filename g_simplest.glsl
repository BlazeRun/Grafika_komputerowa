#version 330 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

in vec3 fragPos[];
in vec3 fragNormal[];
in vec4 fragColor[];

out vec3 gFragPos;
out vec3 gFragNormal;
out vec4 gFragColor;

void main() {
    for (int i = 0; i < 3; ++i) {
        gFragPos = fragPos[i];
        gFragNormal = normalize(fragNormal[i]); // g³adkie cieniowanie
        gFragColor = fragColor[i];
        gl_Position = gl_in[i].gl_Position;
        EmitVertex();
    }
    EndPrimitive();
}
