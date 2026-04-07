#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragPosWorld;
layout(location = 2) in vec3 fragNormalWorld;
layout(location = 3) in vec2 fragUV;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4 projectionViewMatrix;
    vec4 ambientLightColor;
    vec3 lightDirection;
    vec4 lightColor;
    mat4 finalBonesMatrices[100];
    mat4 view;
    mat4 proj;
    mat4 lightSpaceMatrix; 
    
    // ★ 영수증 업데이트: 가위 데이터 받기
    vec4 clipPlane; 
} ubo;

layout(set = 0, binding = 1) uniform sampler2D texSampler;
layout(set = 0, binding = 2) uniform sampler2D shadowMap;

void main() {
    // ==========================================
    // ★ 세상 자르기 (Clipping)
    // 픽셀의 월드 위치와 가위(clipPlane)를 내적(dot)합니다.
    // 결과가 0보다 작으면 "이 픽셀은 자르는 면 너머에 있다!"라고 판단하고 
    // GPU에게 그리지 말고 버리라고(discard) 명령합니다.
    // ==========================================
    if (dot(vec4(fragPosWorld, 1.0), ubo.clipPlane) < 0.0) {
        discard; 
    }

    // (이하 기존 조명 및 그림자 연산 코드는 그대로 유지)
    vec3 surfaceNormal = normalize(fragNormalWorld);
    vec3 textureColor = texture(texSampler, fragUV).rgb;

    vec4 lightSpacePos = ubo.lightSpaceMatrix * vec4(fragPosWorld, 1.0);
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    projCoords.xy = projCoords.xy * 0.5 + 0.5;

    float shadow = 0.0;
    if (projCoords.z > -1.0 && projCoords.z < 1.0) {
        float closestDepth = texture(shadowMap, projCoords.xy).r;
        float currentDepth = projCoords.z;
        vec3 lightDir = normalize(-ubo.lightDirection);
        float bias = max(0.005 * (1.0 - dot(surfaceNormal, lightDir)), 0.001);
        shadow = currentDepth - bias > closestDepth ? 1.0 : 0.0;
    }

    vec3 ambientLight = ubo.ambientLightColor.rgb * ubo.ambientLightColor.a;
    vec3 lightDir = normalize(-ubo.lightDirection);
    float diffuseIntensity = max(dot(surfaceNormal, lightDir), 0.0);
    vec3 diffuseLight = ubo.lightColor.rgb * ubo.lightColor.a * diffuseIntensity;

    vec3 finalColor = textureColor * (ambientLight + (1.0 - shadow) * diffuseLight);
    outColor = vec4(finalColor, 1.0);
}