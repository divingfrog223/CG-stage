// mirror_ball.frag
#version 330 core
out vec4 FragColor;

// PBR材质（和model.frag保持一致）
struct PBRMaterial {
    sampler2D albedoMap;
    sampler2D normalMap;
    sampler2D metallicMap;
    sampler2D roughnessMap;
    sampler2D aoMap;
};

// 聚光灯（和model.frag保持一致）
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

// Uniforms（和model.frag保持一致）
uniform PBRMaterial material;
uniform Light light[8];
uniform vec3 viewPos;

// IBL Uniforms（和model.frag保持一致）
uniform samplerCube irradianceMap;
uniform samplerCube prefilterMap;
uniform sampler2D brdfLUT;

// 镜球特有参数（新增）
uniform vec3 ballColor;           // 基础颜色调色
uniform float glowIntensity;      // 发光强度 1.0-5.0
uniform float sparkleIntensity;   // 闪烁强度 0.0-3.0
uniform float reflectivity;       // 额外反射增强 0.0-1.0
uniform float u_time;            // 时间（用于动画）

// 常量
const float PI = 3.14159265359;
const float MAX_REFLECTION_LOD = 4.0;

// 法线分布函数（GGX）
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

// Fresnel-Schlick近似
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

// Fresnel-Schlick with roughness（用于IBL）
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

// 生成随机数（用于闪烁效果）
float random(vec2 st) {
    return fract(sin(dot(st.xy, vec2(12.9898, 78.233))) * 43758.5453123);
}

// 计算闪烁效果
vec3 calculateSparkle(vec2 texCoord, vec3 normal, vec3 viewDir) {
    vec3 sparkle = vec3(0.0);
    
    if (sparkleIntensity > 0.01) {
        // 增加UV密度模拟更多镜片
        vec2 uv = texCoord * 20.0;
        
        // 让UV随时间轻微移动（模拟旋转）
        uv += vec2(sin(u_time * 0.3), cos(u_time * 0.2)) * 0.05;
        
        // 创建网格效果
        vec2 grid = floor(uv);
        vec2 gridPos = fract(uv) - 0.5;
        
        // 随机亮度
        float rand = random(grid + u_time * 0.1);
        
        // 只有部分像素闪烁
        float sparkleFactor = step(0.95, rand);
        
        // 添加光晕效果
        sparkleFactor *= (1.0 - length(gridPos) * 2.0);
        sparkleFactor = max(0.0, sparkleFactor);
        
        // 闪烁颜色（稍微随机化）
        vec3 sparkleColor = vec3(1.0, 0.95 + rand * 0.1, 0.85);
        sparkle = sparkleColor * sparkleFactor * sparkleIntensity;
    }
    
    return sparkle;
}

// 计算边缘光晕
vec3 calculateRimLight(vec3 normal, vec3 viewDir) {
    float rim = 1.0 - max(dot(normal, viewDir), 0.0);
    rim = smoothstep(0.4, 1.0, rim);
    return vec3(0.7, 0.8, 1.0) * rim * 0.2 * glowIntensity;
}

// 计算自发光
vec3 calculateEmission(vec3 albedo, float metallic) {
    // 金属部分发光更强
    float emissionFactor = metallic * glowIntensity * 1.5;
    return albedo * emissionFactor;
}

void main() {
    // ====== 1. 基础PBR计算（和model.frag相同） ======
    
    // 从贴图采样
    vec3 albedo = texture(material.albedoMap, TexCoords).rgb;
    float metallic = texture(material.metallicMap, TexCoords).r;
    float roughness = texture(material.roughnessMap, TexCoords).r;
    float ao = texture(material.aoMap, TexCoords).r;
    
    // 应用镜球颜色调色
    albedo *= ballColor;
    
    // 增强反射率（镜球应该更反光）
    metallic = clamp(metallic + reflectivity * 0.3, 0.0, 1.0);
    roughness = roughness * (1.0 - reflectivity * 0.5);
    
    // 法线和视图向量
    vec3 N = normalize(Normal);
    vec3 V = normalize(viewPos - FragPos);
    vec3 R = reflect(-V, N);
    
    // 计算F0（基础反射率）
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);
    
    // 直接光照计算（和model.frag相同）
    vec3 Lo = vec3(0.0);
    
    for(int i = 0; i < 8; i++) {
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
    
    // ====== 2. IBL环境光照（和model.frag相同） ======
    vec3 F = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;
    
    // 漫反射部分
    vec3 irradiance = texture(irradianceMap, N).rgb;
    vec3 diffuse = irradiance * albedo;
    
    // 镜面反射部分
    float lod = roughness * MAX_REFLECTION_LOD;
    vec3 prefilteredColor = textureLod(prefilterMap, R, lod).rgb;
    vec2 brdf = texture(brdfLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
    vec3 specular = prefilteredColor * (F * brdf.x + brdf.y);
    
    // 环境光
    vec3 ambient = (kD * diffuse + specular) * ao;
    
    // ====== 3. 镜球特殊效果（新增） ======
    vec3 sparkle = calculateSparkle(TexCoords, N, V);
    vec3 rimLight = calculateRimLight(N, V);
    vec3 emission = calculateEmission(albedo, metallic);
    
    // 聚光灯直射增强
    for(int i = 0; i < 8; i++) {
        vec3 toLight = normalize(light[i].position - FragPos);
        float spotlightFactor = max(dot(-toLight, V), 0.0);
        if (spotlightFactor > 0.98) {
            emission += light[i].color * pow(spotlightFactor, 50.0) * glowIntensity;
        }
    }
    
    // ====== 4. 最终颜色组合 ======
    vec3 color = ambient + Lo;           // 标准PBR
    color += emission;                   // 自发光
    color += sparkle;                    // 闪烁效果
    color += rimLight;                   // 边缘光晕
    
    // ====== 5. 输出 ======
    FragColor = vec4(color, 1.0);
}