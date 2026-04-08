#version 450

layout(location = 0) in vec4 clipSpace;
layout(location = 1) in vec2 fragUV;
layout(location = 2) in vec3 fragPosWorld;

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
    vec4 clipPlane;
    float time;
} ubo;

layout(set = 0, binding = 1) uniform sampler2D reflectionTex;
layout(set = 0, binding = 2) uniform sampler2D refractionTex;
// ★ 텍스처 2장 추가!
layout(set = 0, binding = 3) uniform sampler2D dudvMap;   
layout(set = 0, binding = 4) uniform sampler2D normalMap; 

// 파도 설정값 조절
const float waveStrength = 0.04;
const float shineDamper = 20.0; // 반사광의 좁고 넓음 (높을수록 날카로움)
const float reflectivity = 0.8; // 반사광 강도

void main() {
    vec2 ndc = (clipSpace.xy / clipSpace.w) * 0.5 + 0.5;
    vec2 reflectTexCoords = vec2(ndc.x, 1.0 - ndc.y);
    vec2 refractTexCoords = vec2(ndc.x, ndc.y);

    // 1. DuDv 왜곡 (물이 자연스럽게 흐르도록 교차 애니메이션)
    float moveFactor = ubo.time * 0.05;
    vec2 distortedTexCoords = texture(dudvMap, vec2(fragUV.x + moveFactor, fragUV.y)).rg * 0.1;
    distortedTexCoords = fragUV + vec2(distortedTexCoords.x, distortedTexCoords.y + moveFactor);
    vec2 totalDistortion = (texture(dudvMap, distortedTexCoords).rg * 2.0 - 1.0) * waveStrength;

    reflectTexCoords += totalDistortion;
    refractTexCoords += totalDistortion;
    reflectTexCoords = clamp(reflectTexCoords, 0.001, 0.999);
    refractTexCoords = clamp(refractTexCoords, 0.001, 0.999);

    vec4 reflectColor = texture(reflectionTex, reflectTexCoords);
    vec4 refractColor = texture(refractionTex, refractTexCoords);

    // 2. 노멀 맵에서 굴곡 정보 가져오기
    vec4 normalMapColor = texture(normalMap, distortedTexCoords);
    vec3 normal = vec3(normalMapColor.r * 2.0 - 1.0, normalMapColor.b * 2.5, normalMapColor.g * 2.0 - 1.0);
    normal = normalize(normal);

    // 3. 태양빛 반사(Specular) 계산 (블린-폰 모델 응용)
    // 뷰 행렬(View Matrix)의 역행렬을 이용해 카메라의 현재 월드 위치를 역추적합니다.
    vec3 cameraPos = vec3(inverse(ubo.view)[3]); 
    vec3 viewVector = normalize(cameraPos - fragPosWorld);
    vec3 lightDir = normalize(-ubo.lightDirection);
    
    vec3 reflectedLight = reflect(-lightDir, normal);
    float specular = max(dot(reflectedLight, viewVector), 0.0);
    specular = pow(specular, shineDamper);
    vec3 specularHighlights = ubo.lightColor.rgb * specular * reflectivity;

    // 4. 최종 색상 합성
    vec4 waterColor = mix(reflectColor, refractColor, 0.5);
    waterColor = mix(waterColor, vec4(0.0, 0.3, 0.5, 1.0), 0.2); // 푸른 바다빛 첨가
    
    // 물 색상 위에 태양의 눈부신 하이라이트를 더합니다.
    outColor = waterColor + vec4(specularHighlights, 0.0);
    outColor.a = 1.0;
}