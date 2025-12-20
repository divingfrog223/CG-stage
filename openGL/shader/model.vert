#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 5) in ivec4 aBoneIDs;
layout (location = 6) in vec4 aWeights;

const int MAX_BONES = 100;

out vec2 TexCoords;
out vec3 Normal;
out vec3 FragPos;
//out vec3 WorldPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 boneMatrices[MAX_BONES];
uniform bool useBones = false;

void main()
{
    vec4 finalPosition = vec4(aPos, 1.0);
    vec4 finalNormal = vec4(aNormal, 0.0);

    if (useBones) {
        mat4 boneTransform = mat4(0.0);
        float totalWeight = 0.0;

        for (int i = 0; i < 4; i++) {
            if (aBoneIDs[i] >= 0 && aWeights[i] > 0.0) {
                boneTransform += boneMatrices[aBoneIDs[i]] * aWeights[i];
                totalWeight += aWeights[i];
            }
        }

        if (totalWeight > 0.0) {
            if (totalWeight < 1.0) {
                boneTransform += mat4(1.0) * (1.0 - totalWeight);
            }
            finalPosition = boneTransform * vec4(aPos, 1.0);
            finalNormal = boneTransform * vec4(aNormal, 0.0);
        }
    }

    FragPos = vec3(model * finalPosition);
    Normal = mat3(transpose(inverse(model))) * vec3(finalNormal);
    TexCoords = aTexCoords;

    gl_Position = projection * view * vec4(FragPos, 1.0);
    FragPos = vec3(model * vec4(aPos, 1.0));
}