#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 5) in ivec4 aBoneIDs;
layout (location = 6) in vec4 aWeights;

const int MAX_BONES = 100;

uniform mat4 boneMatrices[MAX_BONES];
uniform bool useBones = false;

out vec2 TexCoords;

out VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
    vec4 FragPosLightSpace;
} vs_out;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;
uniform mat4 lightSpaceMatrix;

void main()
{
    vs_out.FragPos = vec3(model * vec4(aPos, 1.0));

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
           vs_out.FragPos =vec3( boneTransform * vec4(aPos, 1.0));
       }
   }

    vs_out.Normal = transpose(inverse(mat3(model))) * aNormal;
    vs_out.TexCoords = aTexCoords;
    vs_out.FragPosLightSpace = lightSpaceMatrix * vec4(vs_out.FragPos, 1.0);
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}


  