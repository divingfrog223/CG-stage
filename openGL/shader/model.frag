#version 330 core
out vec4 FragColor;

// PBR材质
struct PBRMaterial {
    sampler2D albedoMap;      // 基础颜色/反照率
    sampler2D normalMap;      // 法线贴图
    sampler2D metallicMap;    // 金属度贴图
    sampler2D roughnessMap;  // 粗糙度贴图
    sampler2D aoMap;         // 环境光遮蔽贴图
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

uniform vec3 ballPos;         // 镜球位置
uniform float ballRotation;   // 镜球旋转角度
uniform bool renderSpots;     // 是否渲染光斑开关
uniform vec3 spotColor;       // 光斑颜色

//镜球自发光
uniform vec3 emissiveColor;     // 自发光颜色 (例如 vec3(1.0, 1.0, 1.0))
uniform float emissiveIntensity; // 自发光强度 (通常设为 > 1.0 以触发 Bloom)

// IBL Uniforms - 必需
uniform samplerCube irradianceMap;      // 漫反射辐照度
uniform samplerCube prefilterMap;       // 预滤波镜面反射
uniform sampler2D brdfLUT;              // BRDF查找表

// 常量
const float PI = 3.14159265359;
const float MAX_REFLECTION_LOD = 4.0;

// 简单的伪随机函数
float hash(vec3 p) {
    p = fract(p * 0.3183099 + 0.1);
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

// 简单的旋转函数
vec3 rotateY(vec3 v, float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return vec3(v.x * c - v.z * s, v.y, v.x * s + v.z * c);
}

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
    // 计算自发光项
    vec3 emissive = emissiveColor * emissiveIntensity * albedo * metallic ;

    //光斑
    vec3 finalSpots = vec3(0.0);

  if (renderSpots) {
    vec3 N = normalize(Normal); 
    
    vec3 worldPosToBall = FragPos - ballPos;
    vec3 dir = normalize(worldPosToBall + N * 0.5); // 0.5 是扰动强度

        //vec3 dir = normalize(FragPos - ballPos);
        vec3 rotatedDir = rotateY(dir, -ballRotation);

        // --- 核心优化：打破规则感 ---
        // 1. 缩放坐标
        vec3 p = rotatedDir * 5.0; 
        
        // 2. 使用 floor 锁定每个小方格，给每个格子里生成一个随机点
        vec3 i = floor(p);
        vec3 f = fract(p);
        
        // 在每个格子里计算一个随机位移，让点不再整齐划一
        float h = hash(i);
        
        // 3. 计算点阵：只有随机值超过一定阈值的格子里才会产生光点
        // 这样光点就会疏密有致，而不是铺满地面
        float spot = 0.0;
        if(h > 0.0) { // 调整这个值可以控制点的稀疏程度
            // 计算格内像素到随机点中心的距离
            float distToCenter = length(f - 0.5 + (vec3(h, fract(h*10.0), fract(h*100.0)) - 0.5) * 0.6);
            spot = smoothstep(0.2, 0.05, distToCenter); 
        }

        // --- 增加动态感：色散效果 ---
        // 稍微偏移 R 和 B 通道，让光点边缘有彩色
        vec3 colorSpots = vec3(
            spot * 1.2, 
            spot * (0.9 + h * 0.2), 
            spot * (0.8 + fract(h*10.0) * 0.4)
        );

        // 距离衰减
        float d = distance(FragPos, ballPos);
        //float attenuation = 1.0 / (1.0 + 0.002 * d * d);
	float density = 0.20; 
	float attenuation = exp(-density * d );		//指数衰减

        finalSpots = spotColor * colorSpots * attenuation * 20.0;
    }

    // 将光斑直接加到 PBR 颜色上
    vec3 color = ambient + Lo + finalSpots;

    // 将自发光加到 PBR 结果上
    color = color + emissive;

    // Gamma校正 （加上Bloom，去掉Gamma矫正）
    //color = color / (color + vec3(1.0));
    //color = pow(color, vec3(1.0/2.2));

    FragColor = vec4(color, 1.0);
}