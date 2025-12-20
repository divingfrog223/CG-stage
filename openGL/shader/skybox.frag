#version 330 core
out vec4 FragColor;

in vec3 TexCoords;

uniform samplerCube skybox;
uniform float brightness = 1.0;  // 亮度调节

void main() {
    vec3 color = texture(skybox, TexCoords).rgb * brightness;
    
    // 可选：色调映射
    color = color / (color + vec3(1.0));
    
    FragColor = vec4(color, 1.0);
}