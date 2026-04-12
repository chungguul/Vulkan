#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragPosWorld;
layout(location = 2) in vec3 fragNormalWorld;
layout(location = 3) in vec2 fragUV;
layout(location = 4) in vec3 fragTangentWorld;
layout(location = 5) in vec3 fragBitangentWorld;

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

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}


void main() {
    // 반사/굴절 렌더링을 위한 가위질 (이전 물 그래픽용 코드 유지)
    if (dot(vec4(fragPosWorld, 1.0), ubo.clipPlane) < 0.0) {
        discard; 
    }

    // ★ 1. 감마 보정 (Gamma Correction): 
    // 텍스처 이미지(sRGB)를 그대로 조명 계산에 쓰면 물리가 깨지므로 선형(Linear) 공간으로 변환합니다.
    vec3 albedo = texture(texSampler, fragUV).rgb;
    albedo = pow(albedo, vec3(2.2));

    vec3 N = fragNormalWorld;
    // CPU에서 법선 데이터가 누락되어 (0,0,0)으로 들어오면 강제로 위쪽(0,1,0)으로 띄워줍니다.
    if (length(N) < 0.01) { N = vec3(0.0, 1.0, 0.0); }
    N = normalize(N);

    vec3 T = fragTangentWorld;
    bool isTBad = (length(T) < 0.01);
    
    // 접선이 정상이더라도, 법선(N)과 완전히 평행하면 외적(Cross)이 터지므로 방어합니다.
    if (!isTBad) {
        if (abs(dot(normalize(T), N)) > 0.999) { isTBad = true; }
    }

    // 데이터가 불량하면 셰이더가 임의의 안전한 접선(Tangent)을 창조합니다.
    if (isTBad) {
        vec3 up = abs(N.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(0.0, 0.0, 1.0);
        T = normalize(cross(up, N));
    } else {
        T = normalize(T);
        T = normalize(T - dot(T, N) * N);
    }

    vec3 B = cross(N, T);
    mat3 TBN = mat3(T, B, N);
    // 노멀 맵 텍스처에서 RGB 값을 읽어옵니다.
    vec3 normalMapColor = texture(normalMap, fragUV).rgb;
    normalMapColor = pow(normalMapColor, vec3(1.0/2.2));
    
    // RGB(0.0 ~ 1.0) 범위를 XYZ 방향 벡터(-1.0 ~ 1.0) 범위로 매핑합니다.
    vec3 mappedNormal = normalMapColor * 2.0 - 1.0; 
    
    // 최종 법선(N)을 노멀 맵이 지시하는 방향으로 꺾어버립니다!
    N = normalize(TBN * mappedNormal);
    // ==========================================================

    //vec3 camPos = vec3(inverse(ubo.view)[3]);
    //vec3 V = normalize(camPos - fragPosWorld);

    // ★ 2. PBR 재질 설정
    float metallic = push.metallic;  
    float roughness = max(push.roughness, 0.05); 
    float ao = 1.0;        

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
    float normalOffset = 0.05 * (1.0 - max(dot(N, L), 0.0));
    vec3 biasedFragPos = fragPosWorld + N * normalOffset;

    // 원래 위치(fragPosWorld) 대신, 살짝 띄운 위치(biasedFragPos)로 그림자 맵을 샘플링합니다.
    vec4 lightSpacePos = ubo.lightSpaceMatrix * vec4(biasedFragPos, 1.0);
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    projCoords.xy = projCoords.xy * 0.5 + 0.5;

    float shadow = 0.0;
    if (projCoords.z > -1.0 && projCoords.z < 1.0) {
        float currentDepth = projCoords.z;
        float bias = 0.001; // 이미 위치를 위로 띄웠으므로 기존의 무식한 z-bias는 최소한으로 줄입니다.
        
        vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
        
        // 기존과 동일한 3x3 소프트웨어 PCF 유지
        for(int x = -1; x <= 1; ++x) {
            for(int y = -1; y <= 1; ++y) {
                float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
                shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
            }
        }
        shadow /= 9.0;
    }

    // --- 최종 색상 합성 ---
    // ==========================================================
    // ★ IBL (Image-Based Lighting) 환경광 계산
    // ==========================================================
    // 1. 거칠기(Roughness)를 반영한 프레넬 계산
    vec3 kS_IBL = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
    vec3 kD_IBL = 1.0 - kS_IBL;
    kD_IBL *= 1.0 - metallic;

    // 2. Diffuse IBL (조도 맵)
    vec3 irradiance = texture(irradianceMap, N).rgb; 
    vec3 diffuseIBL = irradiance * albedo;

    // 3. Specular IBL (사전 필터링 맵 + BRDF 근사 적용!)
    vec3 R = reflect(-V, N); 
    const float MAX_REFLECTION_LOD = 4.0; // 밉맵 5단계 (0~4)
    
    // ★ 마법의 함수 textureLod! 거칠기에 비례해서 흐린 밉맵(0.0~4.0)을 쏙쏙 뽑아옵니다.
    vec3 prefilteredColor = textureLod(prefilteredMap, R, roughness * MAX_REFLECTION_LOD).rgb;
    
    // [Pro Tip] IBL 3신기의 마지막 'BRDF LUT' 텍스처를 굽는 대신, 
    // 언리얼 엔진(Epic Games)에서 사용하는 수학적 근사 공식을 사용해 텍스처 메모리를 아낍니다!
    float NdotV = max(dot(N, V), 0.0);
    vec4 c0 = vec4(-1.0, -0.0275, -0.572, 0.022);
    vec4 c1 = vec4(1.0, 0.0425, 1.04, -0.04);
    vec4 r = roughness * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28 * NdotV)) * r.x + r.y;
    vec2 envBRDF = vec2(-1.04, 1.04) * a004 + r.zw;

    // 프레넬(F0)과 환경 BRDF를 곱하여 최종 정반사광 완성
    vec3 specularIBL = prefilteredColor * (F0 * envBRDF.x + vec3(envBRDF.y));

    // 4. 최종 Ambient = 은은한 난반사 + 영롱한 거울 반사
    vec3 ambient = (kD_IBL * diffuseIBL + specularIBL) * ao;
    
    ambient *= 0.2; // 환경광 밝기 미세 조절

    // 그림자 적용하여 최종 색상 합성
    vec3 color = ambient + Lo * (1.0 - shadow); 

    // 톤 매핑 및 감마 보정 (기존 유지)
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2));

    outColor = vec4(color, 1.0);

    //int debugMode = 5; 
    // if (debugMode == 1) {
    //     // [테스트 1] 최종 노멀(N) 벡터 확인
    //     // 정상: 알록달록한 구슬처럼 매끄러운 그라데이션 (보라, 연두, 핑크 등)
    //     // 비정상: 단색이거나 까맣게 나옴 (TBN 또는 노멀 맵 붕괴)
    //     outColor = vec4(N * 0.5 + 0.5, 1.0); 
    // } 
    // else if (debugMode == 2) {
    //     // [테스트 2] 알베도(Albedo) 텍스처 확인
    //     // 정상: 그림자와 빛이 아예 없는 순수한 '나무(Wood)' 텍스처 이미지가 보여야 함
    //     // 비정상: 하얗거나 까맣게 나옴 (텍스처 로딩 또는 바인딩 실패)
    //     outColor = vec4(albedo, 1.0);
    // } 
    // else if (debugMode == 3) {
    //     // [테스트 3] 푸시 상수: 거칠기(Roughness) 값 확인
    //     // 정상: 거칠기가 0.1이면 아주 어두운 회색, 0.9면 아주 밝은 회색으로 나와야 함
    //     outColor = vec4(vec3(roughness), 1.0);
    // } 
    // else if (debugMode == 4) {
    //     // [테스트 4] 푸시 상수: 금속성(Metallic) 값 확인
    //     // 정상: 코로네(1.0)는 순백색, 큐브/구체(0.0)는 순흑색으로 완전히 대비되어야 함
    //     outColor = vec4(vec3(metallic), 1.0);
    // } 
    // else if (debugMode == 5) {
    //     // [테스트 5] 조도 맵(Irradiance) 샘플링 확인
    //     // 정상: 은은한 하늘색/주황색이 부드럽게 섞인 파스텔톤 컬러
    //     // 비정상: 순백색(눈뽕) 또는 노이즈 가득함 (조도 맵 베이킹 버그)
    //     vec3 dbgIrradiance = texture(irradianceMap, N).rgb;
    //     outColor = vec4(dbgIrradiance, 1.0);
    // } 
    // else if (debugMode == 6) {
    //     // [테스트 6] 난반사 IBL(Diffuse IBL) 연산 결과 확인
    //     // 정상: 조도 맵 빛 * 나무 텍스처 색상
    //     outColor = vec4(diffuseIBL, 1.0);
    // } 
    // else {
    //     // [0] 원래 렌더링
    //     outColor = vec4(color, 1.0);
    // }
}