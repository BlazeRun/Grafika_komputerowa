#version 330 core

in vec3  fragPos;
in vec3  fragNormal;
in vec2  fragTexCoord;

out vec4 outColor;

uniform sampler2D  texture0;
uniform bool       isEmissive;
uniform vec3       cameraPos;

uniform vec3       lightPos[2];
uniform vec3       lightColor[2];
uniform samplerCube depthMap[2];
uniform float      far_plane;

const int SAMPLES = 20;
const vec3 sampleOffsetDirections[SAMPLES] = vec3[](
    vec3( 1,  1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1,  1,  1),
    vec3( 1,  1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1,  1, -1),
    vec3( 1,  1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1,  1,  0),
    vec3( 1,  0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1,  0, -1),
    vec3( 0,  1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0,  1, -1)
);

float ShadowCalculation(int idx, vec3 fragPos, vec3 lightPos)
{
    vec3 fragToLight = fragPos - lightPos;
    float currentDepth = length(fragToLight);

    // scale filter radius by view distance
    float viewDist   = length(cameraPos - fragPos);
    float diskRadius = (1.0 + viewDist / far_plane) / 25.0;

    float shadow = 0.0;
    float bias   = 0.05;

    for(int i = 0; i < SAMPLES; ++i) {
        vec3 sampleDir    = fragToLight + sampleOffsetDirections[i] * diskRadius;
        float closestDepth = texture(depthMap[idx], sampleDir).r;
        closestDepth      *= far_plane;   // undo [0,1] mapping
        if(currentDepth - bias > closestDepth)
            shadow += 1.0;
    }
    return shadow / float(SAMPLES);
}

void main() {
    vec4 texColor = texture(texture0, fragTexCoord);
    if(isEmissive) {
        outColor = texColor * vec4(1.5, 1.5, 1.3, 1.0);
        return;
    }

    vec3 norm    = normalize(fragNormal);
    vec3 viewDir = normalize(cameraPos - fragPos);

    float shininess        = 64.0;
    float ambientStrength  = 0.15;
    float specularStrength = 0.3;

    vec3 result = vec3(0.0);

    for(int i = 0; i < 2; ++i) {
        vec3 LP = lightPos[i];
        vec3 LC = lightColor[i];

        // Blinn-Phong
        vec3 L   = normalize(LP - fragPos);
        float diff = max(dot(norm, L), 0.0);
        vec3 H     = normalize(L + viewDir);
        float spec = pow(max(dot(norm, H), 0.0), shininess);

        float dist = length(LP - fragPos);
        float atten = 1.25 / (1.0 + 0.35 * dist + 0.25 * dist * dist);

        vec3 ambient  = ambientStrength * LC;
        vec3 diffuse  = diff * LC;
        vec3 specular = specularStrength * spec * LC;

        ambient  *= atten;
        diffuse  *= atten;
        specular *= atten;

        float shadow = ShadowCalculation(i, fragPos, LP);
        result += ambient + (1.0 - shadow) * (diffuse + specular);
    }

    outColor = vec4(result, 1.0) * texColor;

}
