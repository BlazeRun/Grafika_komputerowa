#version 330 core

in vec3 fragPos;
in vec3 fragNormal;
in vec2 fragTexCoord;

out vec4 outColor;

uniform sampler2D texture0;
uniform vec3 cameraPos;
uniform vec3 lightPos1;
uniform vec3 lightColor1;
uniform vec3 lightPos2;
uniform vec3 lightColor2;
uniform bool isEmissive;

void main() {
    vec4 texColor = texture(texture0, fragTexCoord);

    if (isEmissive) {
        outColor = texColor * vec4(1.5, 1.5, 1.3, 1.0);
        return;
    }

    vec3 norm = normalize(fragNormal);
    vec3 viewDir = normalize(cameraPos - fragPos);

    float shininess = 64.0;
    float ambientStrength = 0.15;
    float specularStrength = 0.3;

    vec3 result = vec3(0.0);

    for (int i = 0; i < 2; i++) {
        vec3 lightPos = i == 0 ? lightPos1 : lightPos2;
        vec3 lightColor = i == 0 ? lightColor1 : lightColor2;

        vec3 lightDir = normalize(lightPos - fragPos);
        vec3 halfwayDir = normalize(lightDir + viewDir);

        float diff = max(dot(norm, lightDir), 0.0);
        float spec = pow(max(dot(norm, halfwayDir), 0.0), shininess);

        float distance = length(lightPos - fragPos);
        float attenuation = 1.25 / (1.0 + 0.35 * distance + 0.25 * distance * distance);

        vec3 ambient = ambientStrength * lightColor;
        vec3 diffuse = diff * lightColor;
        vec3 specular = specularStrength * spec * lightColor;

        ambient *= attenuation;
        diffuse *= attenuation;
        specular *= attenuation;

        result += ambient + diffuse + specular;

    }

    outColor = vec4(result, 1.0) * texColor;
}
