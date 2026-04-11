#version 450

// C++에서 넘겨줄 카메라 유니폼 버퍼 (기존과 동일)
layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4 projectionView;
    vec4 ambientLightColor;
    vec3 lightDirection;
    vec4 lightColor;
    
    // 맨 끝에서 스카이박스용 데이터를 안전하게 꺼내 씁니다.
    mat4 view;
    mat4 proj;
} ubo;


// 프래그먼트 셰이더로 넘길 3D 방향 벡터
layout(location = 0) out vec3 outUVW;

// 정육면체(Cube)의 36개 버텍스 좌표 하드코딩
vec3 positions[36] = vec3[](
    vec3(-1.0,  1.0, -1.0), vec3(-1.0, -1.0, -1.0), vec3( 1.0, -1.0, -1.0),
    vec3( 1.0, -1.0, -1.0), vec3( 1.0,  1.0, -1.0), vec3(-1.0,  1.0, -1.0),

    vec3(-1.0, -1.0,  1.0), vec3(-1.0, -1.0, -1.0), vec3(-1.0,  1.0, -1.0),
    vec3(-1.0,  1.0, -1.0), vec3(-1.0,  1.0,  1.0), vec3(-1.0, -1.0,  1.0),

    vec3( 1.0, -1.0, -1.0), vec3( 1.0, -1.0,  1.0), vec3( 1.0,  1.0,  1.0),
    vec3( 1.0,  1.0,  1.0), vec3( 1.0,  1.0, -1.0), vec3( 1.0, -1.0, -1.0),

    vec3(-1.0, -1.0,  1.0), vec3(-1.0,  1.0,  1.0), vec3( 1.0,  1.0,  1.0),
    vec3( 1.0,  1.0,  1.0), vec3( 1.0, -1.0,  1.0), vec3(-1.0, -1.0,  1.0),

    vec3(-1.0,  1.0, -1.0), vec3( 1.0,  1.0, -1.0), vec3( 1.0,  1.0,  1.0),
    vec3( 1.0,  1.0,  1.0), vec3(-1.0,  1.0,  1.0), vec3(-1.0,  1.0, -1.0),

    vec3(-1.0, -1.0, -1.0), vec3(-1.0, -1.0,  1.0), vec3( 1.0, -1.0, -1.0),
    vec3( 1.0, -1.0, -1.0), vec3(-1.0, -1.0,  1.0), vec3( 1.0, -1.0,  1.0)
);

void main() {
    // 1. 현재 정점의 3D 로컬 좌표를 프래그먼트 셰이더(큐브맵 샘플링 방향)로 전달
    outUVW = positions[gl_VertexIndex];

    //outUVW.y *= -1.0;

    // 2. 뷰 행렬에서 '이동(Translation)' 제거! (카메라가 움직여도 하늘은 제자리)
    mat4 rotView = mat4(mat3(ubo.view)); 
    vec4 clipPos = ubo.proj * rotView * vec4(positions[gl_VertexIndex], 1.0);

    // 3. 깊이(Depth) 강제 최댓값 설정 트릭 (z를 w로 맞추면 원근 나누기 후 깊이가 1.0이 됨)
    gl_Position = clipPos.xyww;
}