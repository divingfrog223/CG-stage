#version 330 core
out vec4 FragColor;

// PBR材质
struct PBRMaterial {
    sampler2D albedoMap;
    sampler2D normalMap;
    sampler2D metallicMap;
    sampler2D roughnessMap;
    sampler2D aoMap;
};

// 聚光灯
struct Light {
    vec3 position;
    vec3 direction;
    vec3 color;
    float strength;
    float constant;
    float linear;
    float quadratic;
    float cutOff;
    float outerCutOff;
    float softEdge;
};

// 输入
in vec2 TexCoords;
in vec3 FragPos;
in vec3 Normal;
//in vec3 WorldPos; 

// Uniforms
uniform PBRMaterial material;
uniform Light light[8];
uniform vec3 viewPos;

// IBL Uniforms - 必需
uniform samplerCube irradianceMap;      // 漫反射辐照度
uniform samplerCube prefilterMap;       // 预滤波镜面反射
uniform sampler2D brdfLUT;              // BRDF查找表

// 常量
const float PI = 3.14159265359;
const float MAX_REFLECTION_LOD = 4.0;

// 法线分布函数（GGX/Trowbridge-Reitz）
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}

// 几何函数（Smith方法）
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx1 = GeometrySchlickGGX(NdotV, roughness);
    float ggx2 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

// Fresnel-Schlick近似 （用于直接光照）
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

// Fresnel-Schlick with roughness（用于IBL）
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

void main() {
    // 从贴图采样
    vec3 albedo = texture(material.albedoMap, TexCoords).rgb;
    float metallic = texture(material.metallicMap, TexCoords).r;
    float roughness = texture(material.roughnessMap, TexCoords).r;
    float ao = texture(material.aoMap, TexCoords).r;
    
    // 法线贴图（简化版，暂时先用法线）
    vec3 N = normalize(Normal);
    vec3 V = normalize(viewPos - FragPos);
    vec3 R = reflect(-V, N); 

    // 计算F0（基础反射率）
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    // 反射方程
    vec3 Lo = vec3(0.0);
    
    for(int i = 0; i < 8; i++) {
        // 聚光灯角度检查
        vec3 lightDir = normalize(light[i].position - FragPos);
        float theta = dot(lightDir, normalize(-light[i].direction));
        
        if(theta > light[i].outerCutOff) {
            // 计算光束强度
            vec3 right = normalize(cross(light[i].direction, vec3(0.0, 1.0, 0.0)));
            vec3 up = normalize(cross(right, light[i].direction));
            vec3 toFrag = FragPos - light[i].position;
            float distRight = dot(toFrag, right);
            float distUp = dot(toFrag, up);
            float radialDistance = sqrt(distRight * distRight + distUp * distUp);
            float beamRadius = length(toFrag) * tan(acos(light[i].cutOff));
            float radialFactor = clamp(radialDistance / beamRadius, 0.0, 1.0);
            float intensity = pow(1.0 - radialFactor, 10.0);
            
            if(intensity > 0.0) {
                // 距离衰减
                float distance = length(light[i].position - FragPos);
                float attenuation = light[i].strength / (0.01 + 0.005 * distance + 0.001 * distance * distance);
                
                // PBR计算
                vec3 L = normalize(light[i].position - FragPos);
                vec3 H = normalize(V + L);
                
                float NDF = DistributionGGX(N, H, roughness);
                float G = GeometrySmith(N, V, L, roughness);
                vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
                
                vec3 numerator = NDF * G * F;
                float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
                vec3 specular = numerator / denominator;
                
                vec3 kS = F;
                vec3 kD = vec3(1.0) - kS;
                kD *= 1.0 - metallic;
                
                float NdotL = max(dot(N, L), 0.0);
                vec3 radiance = light[i].color * attenuation * intensity;
                
                Lo += (kD * albedo / PI + specular) * radiance * NdotL;
            }
        }
    }
    
    // 环境光（简化）
    //vec3 ambient = vec3(0.1) * albedo * ao;

    // ========== IBL 环境光照（完整版本）==========
    // 1. 计算Fresnel项（考虑粗糙度）
    vec3 F = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
    
    // 2. 镜面和漫反射系数
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;  // 非金属才有漫反射
    
    // 3. 漫反射部分 - 从辐照度贴图采样
    vec3 irradiance = texture(irradianceMap, N).rgb;
    vec3 diffuse = irradiance * albedo;
    
    // 4. 镜面反射部分 - 预滤波贴图 + BRDF LUT
    // 计算预滤波的mip级别
    float lod = roughness * MAX_REFLECTION_LOD;
    
    // 采样预滤波贴图
    vec3 prefilteredColor = textureLod(prefilterMap, R, lod).rgb;
    //vec3 prefilteredColor = texture(irradianceMap, R).rgb; 
    
    // 采样BRDF LUT
    vec2 brdf = texture(brdfLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
    
    // 组合镜面反射
    vec3 specular = prefilteredColor * (F * brdf.x + brdf.y);
    
    // 5. 完整的IBL环境光
    vec3 ambient = (kD * diffuse + specular) * ao;

    //vec3 ambient = diffuse * ao ;

    // ====== 删除阴影计算 ======
    // float shadow = ShadowCalculation(FragPos);
    // vec3 color = ambient + (1.0 - shadow) * Lo;
    
    // ====== 直接计算颜色（无阴影） ======
    vec3 color = ambient + Lo;

    // Gamma校正 （加上Bloom，去掉Gamma矫正）
    //color = color / (color + vec3(1.0));
    //color = pow(color, vec3(1.0/2.2));

    FragColor = vec4(color, 1.0);
}