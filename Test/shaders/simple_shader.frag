#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragPosWorld;
layout(location = 2) in vec3 fragNormalWorld;
layout(location = 3) in vec2 fragUV;

layout(location = 0) out vec4 outColor;

// ★ 영수증 순서 맞추기! (Main.cpp의 GlobalUbo와 똑같아야 합니다)
layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4 projectionViewMatrix;
    vec4 ambientLightColor;
    vec3 lightDirection;
    vec4 lightColor;
    mat4 finalBonesMatrices[100];
    mat4 view;
    mat4 proj;
    mat4 lightSpaceMatrix; // 그림자 투영 행렬!
} ubo;

layout(set = 0, binding = 1) uniform sampler2D texSampler; // 오브젝트 텍스처
layout(set = 0, binding = 2) uniform sampler2D shadowMap;  // ★ 그림자 텍스처 추가!

void main() {
    vec3 surfaceNormal = normalize(fragNormalWorld);
    vec3 textureColor = texture(texSampler, fragUV).rgb;

    // ==========================================
    // 1. 그림자 판별 로직
    // ==========================================
    // 빛의 관점에서의 픽셀 위치를 구하고 원근 나눗셈을 합니다.
    vec4 lightSpacePos = ubo.lightSpaceMatrix * vec4(fragPosWorld, 1.0);
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    
    // Vulkan 좌표계 [-1, 1]을 텍스처 좌표계 [0, 1]로 변환
    projCoords.xy = projCoords.xy * 0.5 + 0.5;

    float shadow = 0.0;
    // 텍스처 범위를 벗어나면 그림자가 지지 않도록 처리
    if (projCoords.z > -1.0 && projCoords.z < 1.0) {
        float closestDepth = texture(shadowMap, projCoords.xy).r;
        float currentDepth = projCoords.z;
        
        // 그림자 깨짐(Acne) 방지를 위한 바이어스
        vec3 lightDir = normalize(-ubo.lightDirection);
        float bias = max(0.005 * (1.0 - dot(surfaceNormal, lightDir)), 0.001);
        
        // 내 픽셀 깊이가 빛에서 가장 가까운 깊이보다 멀면 그림자 발생!
        shadow = currentDepth - bias > closestDepth ? 1.0 : 0.0;
    }

    // ==========================================
    // 2. 최종 조명 적용
    // ==========================================
    vec3 ambientLight = ubo.ambientLightColor.rgb * ubo.ambientLightColor.a;
    vec3 lightDir = normalize(-ubo.lightDirection);
    float diffuseIntensity = max(dot(surfaceNormal, lightDir), 0.0);
    vec3 diffuseLight = ubo.lightColor.rgb * ubo.lightColor.a * diffuseIntensity;

    // ★ 핵심: 그림자에 가려진 곳은 직사광선(diffuseLight)을 완전히 꺼버립니다 (1.0 - shadow)
    vec3 finalColor = textureColor * (ambientLight + (1.0 - shadow) * diffuseLight);

    outColor = vec4(finalColor, 1.0);
}