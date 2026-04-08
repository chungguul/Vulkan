#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragPosWorld;
layout(location = 2) in vec3 fragNormalWorld;
layout(location = 3) in vec2 fragUV;

layout(location = 0) out vec4 outColor;

struct PointLight {
    vec4 position; // xyz: 위치, w: 강도
    vec4 color;    // xyz: 색상
};

layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4 projectionViewMatrix;
    vec4 ambientLightColor;
    vec3 lightDirection;
    vec4 lightColor;
    mat4 finalBonesMatrices[100];
    mat4 view;
    mat4 proj;
    mat4 lightSpaceMatrix; 
    vec4 clipPlane; 
    float time;
    PointLight pointLights[10];
    int numPointLights;
} ubo;

layout(push_constant) uniform Push {
    mat4 modelMatrix;
    float roughness;
    float metallic;
} push;

layout(set = 0, binding = 1) uniform sampler2D texSampler;
layout(set = 0, binding = 2) uniform sampler2D shadowMap;

const float PI = 3.14159265359;

// ==========================================
// ★ PBR 마법의 3대 수학 공식
// ==========================================
// 1. D (Normal Distribution): 표면의 미세한 굴곡에 따라 빛이 얼마나 흩어지는가 (GGX)
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / max(denom, 0.0000001); // 0으로 나누기 방지
}

// 2. G (Geometry): 미세한 굴곡 때문에 빛이 자기 자신에게 가려져 그림자가 지는 현상 (Schlick-GGX)
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

// 3. F (Fresnel): 시선 각도에 따라 반사율이 달라지는 현상 (Schlick의 근사식)
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
// ==========================================

void main() {
    // 반사/굴절 렌더링을 위한 가위질 (이전 물 그래픽용 코드 유지)
    if (dot(vec4(fragPosWorld, 1.0), ubo.clipPlane) < 0.0) {
        discard; 
    }

    // ★ 1. 감마 보정 (Gamma Correction): 
    // 텍스처 이미지(sRGB)를 그대로 조명 계산에 쓰면 물리가 깨지므로 선형(Linear) 공간으로 변환합니다.
    vec3 albedo = texture(texSampler, fragUV).rgb;
    albedo = pow(albedo, vec3(2.2));

    // ★ 2. PBR 재질 설정
    float metallic = push.metallic;  
    float roughness = max(push.roughness, 0.05); 
    float ao = 1.0;        

    vec3 N = normalize(fragNormalWorld);
    vec3 camPos = vec3(inverse(ubo.view)[3]); // 뷰 행렬을 역산하여 카메라 월드 위치 추적
    vec3 V = normalize(camPos - fragPosWorld);

    // F0 (기본 반사율): 비금속은 보통 0.04, 금속은 알베도 색상 자체를 사용
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    // 반사되는 빛을 누적할 변수
    vec3 Lo = vec3(0.0);

    // --- 광원 1: 태양광(Directional Light) 계산 ---
    vec3 L = normalize(-ubo.lightDirection);
    vec3 H = normalize(V + L); // 시선과 빛의 중간 벡터
    
    vec3 radiance = ubo.lightColor.rgb * ubo.lightColor.a;

    // Cook-Torrance BRDF 적용
    float NDF = DistributionGGX(N, H, roughness);   
    float G   = GeometrySmith(N, V, L, roughness);      
    vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);       
    
    vec3 numerator    = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;
    
    // 에너지 보존 법칙: 반사된 빛(kS)과 흡수된 빛(kD)의 합은 1을 넘을 수 없습니다.
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic; // 금속은 빛을 흡수하지 않고 모두 반사함

    float NdotL = max(dot(N, L), 0.0);
    Lo += (kD * albedo / PI + specular) * radiance * NdotL;

    for(int i = 0; i < ubo.numPointLights; i++) {
        vec3 lightPos = ubo.pointLights[i].position.xyz;
        float intensity = ubo.pointLights[i].position.w;
        vec3 lightColor = ubo.pointLights[i].color.xyz;

        // 1. 빛의 방향과 거리(Attenuation) 계산 (역제곱 법칙)
        vec3 L_pt = lightPos - fragPosWorld;
        float distance = length(L_pt);
        L_pt = normalize(L_pt);
        vec3 H_pt = normalize(V + L_pt);

        // 거리가 멀어질수록 빛이 급격히 약해집니다.
        float attenuation = 1.0 / (distance * distance); 
        vec3 radiance_pt = lightColor * intensity * attenuation;

        // 2. Cook-Torrance BRDF 적용 (태양광과 동일한 마법 공식)
        float NDF_pt = DistributionGGX(N, H_pt, roughness);   
        float G_pt   = GeometrySmith(N, V, L_pt, roughness);      
        vec3 F_pt    = fresnelSchlick(max(dot(H_pt, V), 0.0), F0);       
        
        vec3 nominator_pt    = NDF_pt * G_pt * F_pt;
        float denominator_pt = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L_pt), 0.0) + 0.0001;
        vec3 specular_pt = nominator_pt / denominator_pt;
        
        vec3 kS_pt = F_pt;
        vec3 kD_pt = vec3(1.0) - kS_pt;
        kD_pt *= 1.0 - metallic;

        float NdotL_pt = max(dot(N, L_pt), 0.0);
        
        // ★ 태양빛(Lo)에 새로운 포인트 라이트 에너지를 계속 누적하여 더합니다!
        Lo += (kD_pt * albedo / PI + specular_pt) * radiance_pt * NdotL_pt;
    }

    // --- 그림자 계산 (기존 코드 유지) ---
    vec4 lightSpacePos = ubo.lightSpaceMatrix * vec4(fragPosWorld, 1.0);
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    projCoords.xy = projCoords.xy * 0.5 + 0.5;

    float shadow = 0.0;
    if (projCoords.z > -1.0 && projCoords.z < 1.0) {
        float closestDepth = texture(shadowMap, projCoords.xy).r;
        float currentDepth = projCoords.z;
        float bias = max(0.005 * (1.0 - dot(N, L)), 0.001);
        shadow = currentDepth - bias > closestDepth ? 1.0 : 0.0;
    }

    // --- 최종 색상 합성 ---
    vec3 ambient = ubo.ambientLightColor.rgb * ubo.ambientLightColor.a * albedo * ao;
    vec3 color = ambient + Lo * (1.0 - shadow);

    // ★ 3. 톤 매핑(Tone Mapping) 및 감마 복원
    // PBR 계산으로 인해 색상이 1.0(흰색)을 넘어가서 눈뽕(?)이 오는 것을 방지(Reinhard)
    color = color / (color + vec3(1.0));
    // 선형 공간에서 계산이 끝났으므로, 모니터가 올바르게 보여줄 수 있도록 sRGB로 되돌립니다.
    color = pow(color, vec3(1.0/2.2));

    outColor = vec4(color, 1.0);
}