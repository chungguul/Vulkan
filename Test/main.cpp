#include <vulkan/vulkan.h>
#include <iostream>
#include <vector>

int main() {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Cross Platform Vulkan";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    std::vector<const char*> extensions;

    // Mac(Apple 플랫폼)에서 컴파일될 때만 MoltenVK 관련 확장을 추가합니다.
#ifdef __APPLE__
    extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkInstance instance;
    if (vkCreateInstance(&createInfo, nullptr, &instance) == VK_SUCCESS) {
        std::cout << "성공: 현재 OS에서 Vulkan 인스턴스가 정상적으로 생성되었습니다!" << std::endl;
    } else {
        std::cerr << "실패: Vulkan 인스턴스 생성 오류" << std::endl;
    }

    vkDestroyInstance(instance, nullptr);
    return 0;
}