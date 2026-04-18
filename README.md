# Custom Vulkan Engine

그래픽스 학습을 위해 직접 구축한 Vulkan 3D 렌더링 엔진입니다. 서드파티 라이브러리를 내장하였기에 추가 설치가 필요합니다.

## 주요 기능
* Vulkan PBR 파이프라인 (Roughness/Metallic)
* IBL (Image-Based Lighting) 조도 맵 및 실시간 반사
* Skeletal Animation (GPU 스키닝)
* Jolt Physics 기반 물리 및 래그돌 시뮬레이션
* EnTT 기반 ECS (Entity Component System)
* 물 반사 및 굴절 (Clip Plane, DUDV)
* JSON 기반 씬(Scene) 동적 로딩

<img width="1126" height="670" alt="image" src="https://github.com/user-attachments/assets/976f7110-b615-40f5-a14b-8f917d9731f7" />


## 필수 요구 사항
* **Vulkan SDK**: 최초 1회 설치 필수 ([다운로드 링크](https://vulkan.lunarg.com/))
* CMake 3.18 이상
* C++17 지원 컴파일러

## 설치 및 빌드
외부 라이브러리 소스를 포함해야 하므로 라이브러리를 최초 1회 다운로드 해야합니다.

\Vulkan\Engine\third_party 디렉터리에서 해당 명령어로 설치합니다.

``` bash
# 1. GLFW
git submodule add https://github.com/glfw/glfw.git

# 2. GLM
git submodule add https://github.com/g-truc/glm.git

# 3. Assimp
git submodule add https://github.com/assimp/assimp.git

# 4. EnTT
git submodule add https://github.com/skypjack/entt.git

# 5. JoltPhysics
git submodule add https://github.com/jrouwe/JoltPhysics.git joltphysics

# 6. nlohmann_json
git submodule add https://github.com/nlohmann/json.git nlohmann_json
```
