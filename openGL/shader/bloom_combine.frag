#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D bloomTexture;
uniform float exposure = 1.0;
uniform float bloomStrength = 1.5; // 新增：控制光晕的梦幻程度

void main() {
    const float gamma = 2.2;
    
    vec3 sceneColor = texture(sceneTexture, TexCoords).rgb;
    vec3 bloomColor = texture(bloomTexture, TexCoords).rgb;
    

    // bloomColor乘上强度系数
    vec3 result = sceneColor + bloomColor * bloomStrength;
    
    // 色调映射
    result = vec3(1.0) - exp(-result * exposure);
    
    // Gamma校正
    result = pow(result, vec3(1.0 / gamma));
    
    FragColor = vec4(result, 1.0);
}