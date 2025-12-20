#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D sceneTexture;
uniform sampler2D bloomTexture;
uniform float exposure = 1.0;

void main() {
    const float gamma = 2.2;
    
    vec3 sceneColor = texture(sceneTexture, TexCoords).rgb;
    vec3 bloomColor = texture(bloomTexture, TexCoords).rgb;
    
    // 合并场景和Bloom效果
    vec3 result = sceneColor + bloomColor;
    
    // 色调映射
    result = vec3(1.0) - exp(-result * exposure);
    
    // Gamma校正
    result = pow(result, vec3(1.0 / gamma));
    
    FragColor = vec4(result, 1.0);
}