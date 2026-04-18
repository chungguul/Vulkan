#version 450

layout(location = 0) in vec4 clipSpace;
layout(location = 1) in vec2 fragUV;
layout(location = 2) in vec3 fragPosWorld;

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

layout(set = 0, binding = 1) uniform sampler2D reflectionTex;
layout(set = 0, binding = 2) uniform sampler2D refractionTex;
layout(set = 0, binding = 3) uniform sampler2D dudvMap;   
layout(set = 0, binding = 4) uniform sampler2D normalMap; 
layout(set = 0, binding = 5) uniform sampler2D depthMap;

const float waveStrength = 0.04;
const float shineDamper = 20.0;
const float reflectivity = 0.8;

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return nom / max(denom, 0.0000001);
}

float LinearizeDepth(float depth) {
    float near = 0.1;
    float far = 1000.0;
    float z = depth * 2.0 - 1.0; 
    return (2.0 * near * far) / (far + near - z * (far - near));
}

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

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    vec2 ndc = (clipSpace.xy / clipSpace.w) * 0.5 + 0.5;
    vec2 reflectTexCoords = vec2(ndc.x, 1.0 - ndc.y);
    vec2 refractTexCoords = vec2(ndc.x, ndc.y);

    float floorDistance = LinearizeDepth(texture(depthMap, refractTexCoords).r);
    float waterDistance = LinearizeDepth(gl_FragCoord.z);
    float waterDepth = floorDistance - waterDistance;

    float edgeAlpha = clamp(waterDepth / 0.5, 0.0, 1.0);

    //DuDv 왜곡
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

    vec4 deepWaterColor = vec4(0.0, 0.3, 0.5, 0.3);
    float visibility = clamp(waterDepth / 10.0, 0.0, 0.8);
    refractColor = mix(refractColor, deepWaterColor, visibility);

    // 노멀 맵 굴곡 정보
    vec4 normalMapColor = texture(normalMap, distortedTexCoords);
    vec3 normal = vec3(normalMapColor.r * 2.0 - 1.0, normalMapColor.b * 2.5, normalMapColor.g * 2.0 - 1.0);
    normal = normalize(normal);

    // 태양빛 반사(Specular) 계산 (블린-폰 모델 응용)
    // 뷰 행렬(View Matrix)의 역행렬을 이용해 카메라의 현재 월드 위치를 역추적합니다.
    vec3 cameraPos = vec3(inverse(ubo.view)[3]); 
    vec3 viewVector = normalize(cameraPos - fragPosWorld);
    vec3 lightDir = normalize(-ubo.lightDirection);
    
    vec3 reflectedLight = reflect(-lightDir, normal);
    float specular = max(dot(reflectedLight, viewVector), 0.0);
    specular = pow(specular, shineDamper);
    vec3 specularHighlights = ubo.lightColor.rgb * specular * reflectivity;

    float refractiveFactor = dot(viewVector, vec3(0.0, 1.0, 0.0)); 
    refractiveFactor = pow(refractiveFactor, 1.5);
    refractiveFactor = clamp(refractiveFactor, 0.0, 1.0);

    // 4. 최종 색상 합성
    vec4 waterColor = mix(reflectColor, refractColor, refractiveFactor);
    waterColor = mix(waterColor, vec4(0.0, 0.3, 0.5, 1.0), 0.2);
    
    //PBR
    float metallic = 0.0;
    float waterRoughness = 0.1;
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, waterColor.rgb, metallic);
    
    vec3 Lo = vec3(0.0);

    //태양광(Directional Light) 계산
    vec3 L_sun = normalize(-ubo.lightDirection);
    vec3 H_sun = normalize(viewVector + L_sun);
    vec3 radiance_sun = ubo.lightColor.rgb * ubo.lightColor.a;

    float NDF_sun = DistributionGGX(normal, H_sun, waterRoughness);   
    float G_sun   = GeometrySmith(normal, viewVector, L_sun, waterRoughness);      
    vec3 F_sun    = fresnelSchlick(max(dot(H_sun, viewVector), 0.0), F0);       
    
    vec3 nominator_sun    = NDF_sun * G_sun * F_sun;
    float denominator_sun = 4.0 * max(dot(normal, viewVector), 0.0) * max(dot(normal, L_sun), 0.0) + 0.0001;
    vec3 specular_sun = nominator_sun / denominator_sun;
    
    vec3 kS_sun = F_sun;
    vec3 kD_sun = vec3(1.0) - kS_sun;
    kD_sun *= 1.0 - metallic;

    float NdotL_sun = max(dot(normal, L_sun), 0.0);
    Lo += (kD_sun * waterColor.rgb / PI + specular_sun) * radiance_sun * NdotL_sun;

    // 다중 동적 광원(Point Lights) 계산 루프
    for(int i = 0; i < ubo.numPointLights; i++) {
        vec3 lightPos = ubo.pointLights[i].position.xyz;
        float intensity = ubo.pointLights[i].position.w;
        vec3 lightColor = ubo.pointLights[i].color.xyz;

        vec3 L_pt = lightPos - fragPosWorld;
        float distance = length(L_pt);
        L_pt = normalize(L_pt);
        vec3 H_pt = normalize(viewVector + L_pt);

        float attenuation = 1.0 / (distance * distance); 
        vec3 radiance_pt = lightColor * intensity * attenuation;

        float NDF_pt = DistributionGGX(normal, H_pt, waterRoughness);   
        float G_pt   = GeometrySmith(normal, viewVector, L_pt, waterRoughness);      
        vec3 F_pt    = fresnelSchlick(max(dot(H_pt, viewVector), 0.0), F0);       
        
        vec3 nominator_pt    = NDF_pt * G_pt * F_pt;
        float denominator_pt = 4.0 * max(dot(normal, viewVector), 0.0) * max(dot(normal, L_pt), 0.0) + 0.0001;
        vec3 specular_pt = nominator_pt / denominator_pt;
        
        vec3 kS_pt = F_pt;
        vec3 kD_pt = vec3(1.0) - kS_pt;
        kD_pt *= 1.0 - metallic;

        float NdotL_pt = max(dot(normal, L_pt), 0.0);
        
        Lo += (kD_pt * waterColor.rgb / PI + specular_pt) * radiance_pt * NdotL_pt;
    }


    vec3 ambient = vec3(0.01) * waterColor.rgb;
    vec3 color = ambient + Lo;

    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2));

    outColor = vec4(color, 1.0);
}