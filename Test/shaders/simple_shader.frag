#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragPosWorld;
layout(location = 2) in vec3 fragNormalWorld;
layout(location = 3) in vec2 fragUV;
layout(location = 4) in vec3 fragTangentWorld;
layout(location = 5) in vec3 fragBitangentWorld;

layout(location = 0) out vec4 outColor;

struct PointLight {
    vec4 position;
    vec4 color; 
};

layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4 projectionViewMatrix;
    vec4 ambientLightColor;
    vec3 lightDirection;
    vec4 lightColor;
    mat4 view;
    mat4 proj;
    mat4 lightSpaceMatrix; 
    vec4 clipPlane; 
    float time;
    PointLight pointLights[10];
    int numPointLights;
} ubo;

layout(set = 0, binding = 1) uniform sampler2D texSampler;
layout(set = 0, binding = 2) uniform sampler2D shadowMap;
layout(set = 0, binding = 3) uniform samplerCube environmentMap;
layout(set = 0, binding = 4) uniform samplerCube irradianceMap;
layout(set = 0, binding = 5) readonly buffer BoneBuffer { mat4 boneMatrices[]; } boneBuffer;
layout(set = 0, binding = 6) uniform samplerCube prefilteredMap;
layout(set = 0, binding = 7) uniform sampler2D normalMap;

layout(push_constant) uniform Push {
    mat4 modelMatrix;
    float roughness;
    float metallic;
} push;

const float PI = 3.14159265359;

// PBR
// D (Normal Distribution): 표면의 미세한 굴곡에 따라 빛이 얼마나 흩어지는가 (GGX)
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

// G (Geometry): 미세한 굴곡 때문에 빛이 자기 자신에게 가려져 그림자가 지는 현상 (Schlick-GGX)
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

// F (Fresnel): 시선 각도에 따라 반사율이 달라지는 현상 (Schlick의 근사식)
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
// ==========================================

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}


void main() {

    //감마 보정 (Gamma Correction): 
    vec3 albedo = texture(texSampler, fragUV).rgb;
    albedo = pow(albedo, vec3(2.2));

    vec3 N = fragNormalWorld;
    if (length(N) < 0.01) { N = vec3(0.0, 1.0, 0.0); }
    N = normalize(N);

    vec3 T = fragTangentWorld;
    bool isTBad = (length(T) < 0.01);
    
    if (!isTBad) {
        if (abs(dot(normalize(T), N)) > 0.999) { isTBad = true; }
    }

    if (isTBad) {
        vec3 up = abs(N.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(0.0, 0.0, 1.0);
        T = normalize(cross(up, N));
    } else {
        T = normalize(T);
        T = normalize(T - dot(T, N) * N);
    }

    vec3 B = cross(N, T);
    mat3 TBN = mat3(T, B, N);
    vec3 normalMapColor = texture(normalMap, fragUV).rgb;
    normalMapColor = pow(normalMapColor, vec3(1.0/2.2));
    
    vec3 mappedNormal = normalMapColor * 2.0 - 1.0; 
    
    N = normalize(TBN * mappedNormal);


    // PBR 재질 설정
    float metallic = push.metallic;  
    float roughness = max(push.roughness, 0.05); 
    float ao = 1.0;        

    vec3 camPos = vec3(inverse(ubo.view)[3]); // 뷰 행렬을 역산하여 카메라 월드 위치 추적
    vec3 V = normalize(camPos - fragPosWorld);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    vec3 Lo = vec3(0.0);

    // 광원 계산
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
    
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic; // 금속은 빛을 흡수하지 않고 모두 반사함

    float NdotL = max(dot(N, L), 0.0);
    Lo += (kD * albedo / PI + specular) * radiance * NdotL;

    for(int i = 0; i < ubo.numPointLights; i++) {
        vec3 lightPos = ubo.pointLights[i].position.xyz;
        float intensity = ubo.pointLights[i].position.w;
        vec3 lightColor = ubo.pointLights[i].color.xyz;

        //빛의 방향과 거리(Attenuation) 계산 
        vec3 L_pt = lightPos - fragPosWorld;
        float distance = length(L_pt);
        L_pt = normalize(L_pt);
        vec3 H_pt = normalize(V + L_pt);

        // 거리가 멀수록 약하게
        float attenuation = 1.0 / (distance * distance); 
        vec3 radiance_pt = lightColor * intensity * attenuation;

        //Cook-Torrance BRDF 적용
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
        
        Lo += (kD_pt * albedo / PI + specular_pt) * radiance_pt * NdotL_pt;
    }

    // 그림자 계산
    float normalOffset = 0.05 * (1.0 - max(dot(N, L), 0.0));
    vec3 biasedFragPos = fragPosWorld + N * normalOffset;

    vec4 lightSpacePos = ubo.lightSpaceMatrix * vec4(biasedFragPos, 1.0);
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    projCoords.xy = projCoords.xy * 0.5 + 0.5;

    float shadow = 0.0;
    if (projCoords.z > -1.0 && projCoords.z < 1.0) {
        float currentDepth = projCoords.z;
        float bias = 0.001;
        
        vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
        
        //PCF
        for(int x = -1; x <= 1; ++x) {
            for(int y = -1; y <= 1; ++y) {
                float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
                shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
            }
        }
        shadow /= 9.0;
    }

    // 최종 색상 합성
    // 프레넬 계산
    vec3 kS_IBL = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
    vec3 kD_IBL = 1.0 - kS_IBL;
    kD_IBL *= 1.0 - metallic;

    // Diffuse IBL
    vec3 irradiance = texture(irradianceMap, N).rgb; 
    vec3 diffuseIBL = irradiance * albedo;

    // Specular IBL
    vec3 R = reflect(-V, N); 
    const float MAX_REFLECTION_LOD = 4.0; // 밉맵 5단계 (0~4)
    
    vec3 prefilteredColor = textureLod(prefilteredMap, R, roughness * MAX_REFLECTION_LOD).rgb;
    
    // 수학적 근사 공식을 사용
    float NdotV = max(dot(N, V), 0.0);
    vec4 c0 = vec4(-1.0, -0.0275, -0.572, 0.022);
    vec4 c1 = vec4(1.0, 0.0425, 1.04, -0.04);
    vec4 r = roughness * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28 * NdotV)) * r.x + r.y;
    vec2 envBRDF = vec2(-1.04, 1.04) * a004 + r.zw;

    // 프레넬(F0)과 환경 BRDF를 곱하여 최종 정반사광 완성
    vec3 specularIBL = prefilteredColor * (F0 * envBRDF.x + vec3(envBRDF.y));

    vec3 ambient = (kD_IBL * diffuseIBL + specularIBL) * ao;
    
    ambient *= 0.2;

    // 그림자 적용
    vec3 color = ambient + Lo * (1.0 - shadow); 

    // 톤 매핑 및 감마 보정
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2));

    outColor = vec4(color, 1.0);
}