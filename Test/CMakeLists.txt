cmake_minimum_required(VERSION 3.15)
project(VulkanEngine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    set(ENV{VULKAN_SDK} "/Users/dohyun/VulkanSDK/1.4.341.1/macOS")
    set(Vulkan_INCLUDE_DIR "$ENV{VULKAN_SDK}/include")
    set(Vulkan_LIBRARY "$ENV{VULKAN_SDK}/lib/libvulkan.dylib")
endif()

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)

# src 폴더 안의 모든 .cpp 파일을 찾아서 변수에 저장합니다.
file(GLOB_RECURSE SRC_FILES "src/*.cpp")

# 찾은 파일들로 실행 파일을 만듭니다.
add_executable(${PROJECT_NAME} ${SRC_FILES})

# src 폴더를 헤더 검색 경로에 추가합니다.
target_include_directories(${PROJECT_NAME} PRIVATE src ${Vulkan_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} Vulkan::Vulkan glfw glm::glm)

