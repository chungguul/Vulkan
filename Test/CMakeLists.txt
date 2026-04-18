cmake_minimum_required(VERSION 3.18)
project(VulkanEngine)

if(MSVC)
    # 1. 최신 CMake 방식: 전체 프로젝트가 동적 런타임(MD/MDd)을 사용하도록 강제합니다.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    
    # C++ 표준 설정
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zc:__cplusplus")

    # ==========================================================
    # ★ [NEW] 구형 CMake 방식을 쓰는 외부 라이브러리들의 하드코딩 무력화!
    # 모든 컴파일러 플래그를 순회하며 정적(/MT) 세팅을 동적(/MD)으로 강제 변환합니다.
    set(CompilerFlags
        CMAKE_CXX_FLAGS
        CMAKE_CXX_FLAGS_DEBUG
        CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_MINSIZEREL
        CMAKE_CXX_FLAGS_RELWITHDEBINFO
        CMAKE_C_FLAGS
        CMAKE_C_FLAGS_DEBUG
        CMAKE_C_FLAGS_RELEASE
        CMAKE_C_FLAGS_MINSIZEREL
        CMAKE_C_FLAGS_RELWITHDEBINFO
    )
    foreach(CompilerFlag ${CompilerFlags})
        string(REPLACE "/MT" "/MD" ${CompilerFlag} "${${CompilerFlag}}")
        string(REPLACE "/MTd" "/MDd" ${CompilerFlag} "${${CompilerFlag}}")
    endforeach()
    # ==========================================================
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# =========================================================================
# 1. 시스템 전역 의존성 (오직 Vulkan SDK만 시스템에서 찾습니다)
# =========================================================================
if(APPLE)
    set(ENV{VULKAN_SDK} "/Users/dohyun/VulkanSDK/1.4.341.1/macOS")
    set(Vulkan_INCLUDE_DIR "$ENV{VULKAN_SDK}/include")
    set(Vulkan_LIBRARY "$ENV{VULKAN_SDK}/lib/libvulkan.dylib")
elseif(WIN32)
    # Windows는 VULKAN_SDK 환경 변수 자동 인식
endif()

find_package(Vulkan REQUIRED)

# =========================================================================
# 2. 내장형(Vendored) Third Party 라이브러리 세팅
# =========================================================================
# 외부 라이브러리들의 불필요한 예제, 테스트, 문서 빌드를 강제로 꺼서 빌드 속도를 극대화합니다.

# --- GLFW ---
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/glfw)

# --- GLM (Header only) ---
set(GLM_TEST_ENABLE OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/glm)

# --- Assimp ---
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(INJECT_DEBUG_POSTFIX OFF CACHE BOOL "" FORCE) # Debug 빌드 시 d 붙는 것 방지
add_subdirectory(third_party/assimp)

# --- EnTT (Header only) ---
set(ENTT_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ENTT_BUILD_DOCS OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/entt)

# --- Jolt Physics ---
set(TARGET_UNIT_TESTS OFF CACHE BOOL "Disable Jolt unit tests" FORCE)
set(TARGET_HELLO_WORLD OFF CACHE BOOL "Disable Jolt hello world" FORCE)
set(TARGET_PERFORMANCE_TEST OFF CACHE BOOL "Disable Jolt performance tests" FORCE)
set(USE_STATIC_MSVC_RUNTIME OFF CACHE BOOL "Use dynamic MSVC runtime" FORCE)
add_subdirectory(third_party/joltphysics/Build) # Jolt는 Build 폴더 안에 CMake가 있습니다.

# --- JSON (Header only) ---
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/nlohmann_json)


if(MSVC)
    # 런타임 고집을 부리는 외부 타겟들의 리스트를 작성합니다.
    set(VENDOR_TARGETS Jolt assimp glfw zlibstatic zlib)
    
    foreach(TARGET_NAME IN LISTS VENDOR_TARGETS)
        if(TARGET ${TARGET_NAME})
            # 타겟이 생성된 직후, 우리가 직접 속성을 덮어씌워 강제로 MD / MDd 로 고정시킵니다!
            set_target_properties(${TARGET_NAME} PROPERTIES 
                MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
            )
        endif()
    endforeach()
endif()

# =========================================================================
# 3. 셰이더 자동 컴파일 세팅
# =========================================================================
find_program(GLSLC_CMD NAMES glslc
             HINTS "$ENV{VULKAN_SDK}/bin" "$ENV{VULKAN_SDK}/Bin"
             PATHS "/Users/dohyun/VulkanSDK/1.4.341.1/macOS/bin")

if(NOT GLSLC_CMD)
    message(FATAL_ERROR "glslc compiler not found! Please check your VULKAN_SDK environment variable.")
endif()

set(SHADER_DIR "${CMAKE_SOURCE_DIR}/shaders")

add_custom_command(
    OUTPUT "${SHADER_DIR}/vert.spv"
    COMMAND ${GLSLC_CMD} "${SHADER_DIR}/simple_shader.vert" -o "${SHADER_DIR}/vert.spv"
    DEPENDS "${SHADER_DIR}/simple_shader.vert"
)

add_custom_command(
    OUTPUT "${SHADER_DIR}/frag.spv"
    COMMAND ${GLSLC_CMD} "${SHADER_DIR}/simple_shader.frag" -o "${SHADER_DIR}/frag.spv"
    DEPENDS "${SHADER_DIR}/simple_shader.frag"
)

add_custom_target(CompileShaders ALL DEPENDS "${SHADER_DIR}/vert.spv" "${SHADER_DIR}/frag.spv")


# =========================================================================
# 4. 메인 엔진 타겟 빌드
# =========================================================================
file(GLOB_RECURSE SRC_FILES "src/*.cpp")

add_executable(${PROJECT_NAME} ${SRC_FILES})
add_dependencies(${PROJECT_NAME} CompileShaders)

target_include_directories(${PROJECT_NAME} PRIVATE src ${Vulkan_INCLUDE_DIRS})

# 이제 시스템에 깔린 라이브러리를 찾는 것이 아니라, 내가 품은 폴더 안의 타겟들과 바로 연결됩니다!
target_link_libraries(${PROJECT_NAME} PRIVATE 
    Vulkan::Vulkan 
    glfw 
    glm::glm 
    assimp
    EnTT::EnTT 
    Jolt
    nlohmann_json::nlohmann_json
)