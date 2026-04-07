#version 450

layout(location = 0) in vec4 clipSpace;
layout(location = 1) in vec2 fragUV;

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

// ★ 디스크립터 매니저의 기존 레이아웃을 그대로 재활용!
// Binding 1에는 반사 사진, Binding 2에는 굴절 사진이 들어옵니다.
layout(set = 0, binding = 1) uniform sampler2D reflectionTex;
layout(set = 0, binding = 2) uniform sampler2D refractionTex;

void main() {
    // 1. 화면의 픽셀 좌표(NDC)를 텍스처 좌표(0~1)로 변환
    vec2 ndc = (clipSpace.xy / clipSpace.w) * 0.5 + 0.5;
    
    // 반사된 모습은 물에 비친 거울이므로 Y축을 뒤집어서 읽습니다!
    vec2 reflectTexCoords = vec2(ndc.x, 1.0 - ndc.y);
    vec2 refractTexCoords = vec2(ndc.x, ndc.y);

    // 2. 사인 함수를 이용한 마법의 수학 파도 (Procedural Distortion)
    // 시간이 지남에 따라 UV 좌표를 요동치게 만듭니다.
    float distortion = sin(fragUV.x * 20.0 + ubo.time * 2.0) * 0.01 + 
                       cos(fragUV.y * 20.0 + ubo.time * 1.5) * 0.01;
    
    reflectTexCoords += distortion;
    refractTexCoords += distortion;

    // 사진 바깥을 읽지 않도록 제한
    reflectTexCoords = clamp(reflectTexCoords, 0.001, 0.999);
    refractTexCoords = clamp(refractTexCoords, 0.001, 0.999);

    // 3. 왜곡된 좌표로 사진에서 색상을 뽑아옵니다.
    vec4 reflectColor = texture(reflectionTex, reflectTexCoords);
    vec4 refractColor = texture(refractionTex, refractTexCoords);

    // 4. 물 색깔 (청록색)
    vec4 waterColor = vec4(0.0, 0.3, 0.5, 1.0);
    
    // 5. 프레넬(Fresnel) 효과 모방: 멀리 볼수록 반사(거울)가 강해지고, 가까이 볼수록 굴절(투명)이 강해집니다.
    // 일단은 반사와 굴절을 50:50으로 섞고, 파란색을 살짝 첨가합니다.
    outColor = mix(reflectColor, refractColor, 0.5);
    outColor = mix(outColor, waterColor, 0.2);
}