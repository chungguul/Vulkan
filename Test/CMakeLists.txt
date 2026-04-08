cmake_minimum_required(VERSION 3.18)
project(VulkanEngine)

if(MSVC)
    # 전체 프로젝트가 동적 런타임(MD/MDd)을 사용하도록 강제합니다.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    # C++ 표준 설정
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zc:__cplusplus")
endif()


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- 1. 플랫폼별 Vulkan SDK 설정 ---
if(APPLE)
    set(ENV{VULKAN_SDK} "/Users/dohyun/VulkanSDK/1.4.341.1/macOS")
    set(Vulkan_INCLUDE_DIR "$ENV{VULKAN_SDK}/include")
    set(Vulkan_LIBRARY "$ENV{VULKAN_SDK}/lib/libvulkan.dylib")
elseif(WIN32)
    # Windows는 VULKAN_SDK 환경 변수 자동 인식
endif()

find_package(Vulkan REQUIRED)
if(WIN32)
    find_package(glfw3 CONFIG REQUIRED)
    find_package(glm CONFIG REQUIRED)
    find_package(assimp CONFIG REQUIRED)
    find_package(Jolt CONFIG REQUIRED)
    find_package(nlohmann_json CONFIG REQUIRED)
elseif(APPLE) # 추후 패키지 설치 및 경로 수정 필요
    find_package(glfw3 REQUIRED)
    find_package(glm REQUIRED)
    find_package(assimp REQUIRED)
endif()

# --- ★ 추가: EnTT 라이브러리 자동 다운로드 및 포함 ---
# (Configure 시점에 한 번만 다운로드되어 build/_deps에 캐싱됩니다)
include(FetchContent)
FetchContent_Declare(
    entt
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG v3.13.0
)
FetchContent_MakeAvailable(entt)
# -----------------------------------------------------


# --- ★ 새로 추가: Jolt Physics 라이브러리 자동 다운로드 ---
# Jolt의 불필요한 테스트 파일이나 샘플 프로젝트는 빌드하지 않도록 끕니다.
# set(TARGET_UNIT_TESTS OFF CACHE BOOL "Disable Jolt unit tests" FORCE)
# set(TARGET_HELLO_WORLD OFF CACHE BOOL "Disable Jolt hello world" FORCE)
# set(TARGET_PERFORMANCE_TEST OFF CACHE BOOL "Disable Jolt performance tests" FORCE)

# set(USE_STATIC_MSVC_RUNTIME OFF CACHE BOOL "Use dynamic MSVC runtime" FORCE)



# ----------------------------------------------------------


# --- 2. 셰이더 자동 컴파일 세팅 ---
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

# --- 3. 빌드 타겟 설정 ---
file(GLOB_RECURSE SRC_FILES "src/*.cpp")

add_executable(${PROJECT_NAME} ${SRC_FILES})
add_dependencies(${PROJECT_NAME} CompileShaders)

target_include_directories(${PROJECT_NAME} PRIVATE src ${Vulkan_INCLUDE_DIRS})

# ★ EnTT::EnTT 링크 추가!
target_link_libraries(${PROJECT_NAME} PRIVATE 
    Vulkan::Vulkan 
    glfw 
    glm::glm 
    assimp::assimp
    EnTT::EnTT 
    Jolt::Jolt
    nlohmann_json::nlohmann_json
)