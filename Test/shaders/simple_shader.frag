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
} ubo;

//디스크립터 셋을 통해 들어올 텍스쳐 데이터
layout(set = 0, binding = 1) uniform sampler2D texSampler;

void main() {
    vec3 surfaceNormal = normalize(fragNormalWorld);

    //텍스쳐 이미지에서 UV좌표에 해당하는 색을 가져옴
    vec3 textureColor = texture(texSampler, fragUV).rgb;

    // 1. 앰비언트 라이트 (기본 밝기 설정)
    vec3 ambientLight = ubo.ambientLightColor.rgb * ubo.ambientLightColor.a;

    // 2. 디퓨즈 라이트 (직사광선 각도 계산)
    // 태양빛의 방향의 반대(-lightDirection)와 표면 법선의 내적(dot)을 구합니다.
    vec3 lightDir = normalize(-ubo.lightDirection);
    float diffuseIntensity = max(dot(surfaceNormal, lightDir), 0.0); // 0.0 이하(뒷면)는 완전한 그림자로 처리
    vec3 diffuseLight = ubo.lightColor.rgb * ubo.lightColor.a * diffuseIntensity;

    // 3. 최종 색상 = 텍스처 색상 * (앰비언트 + 디퓨즈)
    vec3 finalColor = textureColor * (ambientLight + diffuseLight);

    outColor = vec4(finalColor, 1.0);
}