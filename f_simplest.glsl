#version 330 core

in vec3 fragPos;
in vec3 fragNormal;
in vec4 fragColor;

out vec4 outColor;

uniform vec3 cameraPos;
uniform vec3 lightPos1, lightColor1;
uniform vec3 lightPos2, lightColor2;
uniform bool isEmissive;

void main() {
    if (isEmissive) {
        outColor = fragColor * vec4(2.0, 2.0, 2.0, 1.0); // jasny kolor w³asny
        return;
    }

    vec3 norm = normalize(fragNormal);
    vec3 viewDir = normalize(cameraPos - fragPos);

    // Œwiat³o 1
    vec3 lightDir1 = normalize(lightPos1 - fragPos);
    float diff1 = max(dot(norm, lightDir1), 0.0);
    vec3 diffuse1 = diff1 * lightColor1;
    vec3 reflectDir1 = reflect(-lightDir1, norm);
    float spec1 = pow(max(dot(viewDir, reflectDir1), 0.0), 64.0);
    vec3 specular1 = spec1 * lightColor1;

    // Œwiat³o 2
    vec3 lightDir2 = normalize(lightPos2 - fragPos);
    float diff2 = max(dot(norm, lightDir2), 0.0);
    vec3 diffuse2 = diff2 * lightColor2;
    vec3 reflectDir2 = reflect(-lightDir2, norm);
    float spec2 = pow(max(dot(viewDir, reflectDir2), 0.0), 64.0);
    vec3 specular2 = spec2 * lightColor2;

    vec3 ambient = 0.2 * (lightColor1 + lightColor2);
    vec3 finalColor = ambient + (diffuse1 + diffuse2) + 0.6 * (specular1 + specular2);

    outColor = vec4(finalColor, 1.0) * fragColor;
}
