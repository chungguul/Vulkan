cmake_minimum_required(VERSION 3.15)
project(VulkanEngine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- 1. 플랫폼별 Vulkan SDK 설정 ---
if(APPLE)
    # Mac 전용 환경 변수 강제 주입 (기존 코드 유지)
    set(ENV{VULKAN_SDK} "/Users/dohyun/VulkanSDK/1.4.341.1/macOS")
    set(Vulkan_INCLUDE_DIR "$ENV{VULKAN_SDK}/include")
    set(Vulkan_LIBRARY "$ENV{VULKAN_SDK}/lib/libvulkan.dylib")
elseif(WIN32)
    # Windows에서는 Vulkan SDK 설치 시 VULKAN_SDK 환경 변수가 자동으로 설정됩니다.
    # 따라서 별도의 수동 경로 지정 없이 아래 find_package에서 자동으로 찾게 됩니다.
endif()

find_package(Vulkan REQUIRED)
if(WIN32)
    # 윈도우(vcpkg) 환경: CONFIG 모드를 사용하는 것이 확실하고 안전함
    find_package(glfw3 CONFIG REQUIRED)
    find_package(glm CONFIG REQUIRED)
    find_package(assimp CONFIG REQUIRED)
elseif(APPLE)
    # 맥 환경: 기본 CMake 모듈 탐색 방식 사용
    find_package(glfw3 REQUIRED)
    find_package(glm REQUIRED)
    find_package(assimp REQUIRED)
endif()

# --- 2. 셰이더 자동 컴파일 세팅 (크로스 플랫폼 적용) ---
# OS에 상관없이 환경 변수를 기반으로 glslc 실행 파일을 찾습니다.
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
# -------------------------------

# src 폴더 안의 모든 .cpp 파일을 찾아서 변수에 저장합니다.
file(GLOB_RECURSE SRC_FILES "src/*.cpp")

# 찾은 파일들로 실행 파일을 만듭니다.
add_executable(${PROJECT_NAME} ${SRC_FILES})

# 실행 파일(VulkanEngine)을 빌드하기 전에 무조건 셰이더부터 컴파일하도록 강제합니다!
add_dependencies(${PROJECT_NAME} CompileShaders)

# src 폴더를 헤더 검색 경로에 추가합니다.
target_include_directories(${PROJECT_NAME} PRIVATE src ${Vulkan_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE 
    Vulkan::Vulkan 
    glfw 
    glm::glm 
    assimp::assimp
)