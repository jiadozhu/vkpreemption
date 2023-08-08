/*
 * *
 * * Copyright (C) 2023 Advanced Micro Devices, Inc.
 * *
 * */

#pragma once

#include <vulkan/vulkan.h>
#include "VulkanTools.h"

#include <map>
#include <set>
#include <utility>

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
#define LOG(...) ((void)__android_log_print(ANDROID_LOG_INFO, "vulkanExample", __VA_ARGS__))
#else
#define LOG(...) { printf(__VA_ARGS__); fflush(stdout); }
#endif

struct QueueInfo {
    VkQueueFlagBits type;
    VkQueueGlobalPriorityEXT priority;
    VkQueue queue;
    uint32_t familyIndex;
    unsigned offset;
};

class Workload {
public:
    virtual VkFence submit() = 0;
    virtual void queryTimestamp(uint64_t time_stamp[], int count) = 0;
    virtual void waitIdle() = 0;
};

class Base {
    VkInstance m_instance;
    VkDevice m_device;
    VkPhysicalDevice m_physicalDevice;
    VkPhysicalDeviceProperties m_deviceProperties;
    std::map<VkQueueGlobalPriorityEXT, QueueInfo> m_graphicQueues;
    std::map<VkQueueGlobalPriorityEXT, QueueInfo> m_computeQueues;

    std::map<VkQueueGlobalPriorityEXT, QueueInfo>& GetQueueInfos(VkQueueFlagBits type) {
        switch(type) {
        case VK_QUEUE_COMPUTE_BIT: return m_computeQueues;
        case VK_QUEUE_GRAPHICS_BIT: return m_graphicQueues;
        default: LOG("Unsupported queue type\n");
        }

        return m_graphicQueues;
    }
    QueueInfo& CreateQueueInfo(VkQueueFlagBits type, VkQueueGlobalPriorityEXT priority) {
        QueueInfo queueInfo = {};
        queueInfo.type = type;
        queueInfo.priority = priority;

        return GetQueueInfos(type).insert({priority, queueInfo}).first->second;
    }

public:
    VkDevice GetDevice() const { return m_device; }
    VkInstance GetInstance() const { return m_instance; }
    VkPhysicalDevice GetPhysicalDevice() const { return m_physicalDevice; }
    VkPhysicalDeviceProperties GetPhysicalDeviceProperties() const { return m_deviceProperties; }
    QueueInfo const& GetQueueInfo(VkQueueFlagBits type, VkQueueGlobalPriorityEXT priority) {
        return GetQueueInfos(type).at(priority);
    }

    Base(std::vector<VkQueueGlobalPriorityEXT> graphicPriorities, std::vector<VkQueueGlobalPriorityEXT> computePriorities)
    {
		LOG("Create a device\n");

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
		LOG("loading vulkan lib");
		vks::android::loadVulkanLibrary();
#endif

		VkApplicationInfo appInfo = {};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Vulkan headless example";
		appInfo.pEngineName = "ComputeWork";
		appInfo.apiVersion = VK_API_VERSION_1_0;

		/*
			Vulkan instance creation (without surface extensions)
		*/
		VkInstanceCreateInfo instanceCreateInfo = {};
		instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		instanceCreateInfo.pApplicationInfo = &appInfo;

		uint32_t layerCount = 0;
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
		const char* validationLayers[] = { "VK_LAYER_GOOGLE_threading",	"VK_LAYER_LUNARG_parameter_validation",	"VK_LAYER_LUNARG_object_tracker","VK_LAYER_LUNARG_core_validation",	"VK_LAYER_LUNARG_swapchain", "VK_LAYER_GOOGLE_unique_objects" };
		layerCount = 6;
#else
		const char* validationLayers[] = { "VK_LAYER_LUNARG_standard_validation" };
		layerCount = 1;
#endif
#if DEBUG
		// Check if layers are available
		uint32_t instanceLayerCount;
		vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr);
		std::vector<VkLayerProperties> instanceLayers(instanceLayerCount);
		vkEnumerateInstanceLayerProperties(&instanceLayerCount, instanceLayers.data());

		bool layersAvailable = true;
		for (auto layerName : validationLayers) {
			bool layerAvailable = false;
			for (auto instanceLayer : instanceLayers) {
				if (strcmp(instanceLayer.layerName, layerName) == 0) {
					layerAvailable = true;
					break;
				}
			}
			if (!layerAvailable) {
				layersAvailable = false;
				break;
			}
		}

		if (layersAvailable) {
			instanceCreateInfo.ppEnabledLayerNames = validationLayers;
			const char *validationExt = VK_EXT_DEBUG_REPORT_EXTENSION_NAME;
			instanceCreateInfo.enabledLayerCount = layerCount;
			instanceCreateInfo.enabledExtensionCount = 1;
			instanceCreateInfo.ppEnabledExtensionNames = &validationExt;
		}
#endif
		VK_CHECK_RESULT(vkCreateInstance(&instanceCreateInfo, nullptr, &m_instance));

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
		vks::android::loadVulkanFunctions(m_instance);
#endif
#if DEBUG
		if (layersAvailable) {
			VkDebugReportCallbackCreateInfoEXT debugReportCreateInfo = {};
			debugReportCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
			debugReportCreateInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
			debugReportCreateInfo.pfnCallback = (PFN_vkDebugReportCallbackEXT)debugMessageCallback;

			// We have to explicitly load this function.
			PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT = reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(vkGetInstanceProcAddr(m_instance, "vkCreateDebugReportCallbackEXT"));
			assert(vkCreateDebugReportCallbackEXT);
			VK_CHECK_RESULT(vkCreateDebugReportCallbackEXT(instance, &debugReportCreateInfo, nullptr, &debugReportCallback));
		}
#endif

		/*
			Vulkan device creation
		*/
		// Physical device (always use first)
		uint32_t deviceCount = 0;
		VK_CHECK_RESULT(vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr));
		std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
		VK_CHECK_RESULT(vkEnumeratePhysicalDevices(m_instance, &deviceCount, physicalDevices.data()));
		m_physicalDevice = physicalDevices[0];

		vkGetPhysicalDeviceProperties(m_physicalDevice, &m_deviceProperties);
		LOG("GPU: %s\n", m_deviceProperties.deviceName);

		const float defaultQueuePriority(0.0f);
		uint32_t queueFamilyCount;
		vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, nullptr);
		std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, queueFamilyProperties.data());
        const size_t queueCount = graphicPriorities.size() + computePriorities.size();
        const size_t familyCount = queueFamilyProperties.size();
        std::vector<unsigned> queueFamilyNextOffset(familyCount, 0);
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos(familyCount);
        std::vector<VkDeviceQueueGlobalPriorityCreateInfoEXT > queuePriorityCreateInfos(familyCount);

        for (size_t i = 0; i < familyCount; i++) {
            VkDeviceQueueGlobalPriorityCreateInfoEXT queuePriorityCreateInfo = {};
            VkDeviceQueueCreateInfo queueCreateInfo = {};

            queuePriorityCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_GLOBAL_PRIORITY_CREATE_INFO_EXT;
            queuePriorityCreateInfo.pNext = nullptr;
            queuePriorityCreateInfo.globalPriority = VK_QUEUE_GLOBAL_PRIORITY_RANGE_SIZE_EXT;
            queuePriorityCreateInfos[i] = queuePriorityCreateInfo;

            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = static_cast<uint32_t>(i);
            queueCreateInfo.pNext = nullptr;
            queueCreateInfo.queueCount = 0;
            queueCreateInfo.pQueuePriorities = &defaultQueuePriority;
            queueCreateInfos[i] = queueCreateInfo;
        }

        auto addQueue = [&](VkQueueFlagBits type, VkQueueGlobalPriorityEXT globalPriority) {
            for (uint32_t i = type - 1; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++) {
                printf("addQueue queueFamilyNextOffset:%d queueCount:%d queueFlags:%08x queuetype:%d\n",
                    queueFamilyNextOffset[i], queueFamilyProperties[i].queueCount,
                    queueFamilyProperties[i].queueFlags, type);
                if ((queueFamilyNextOffset[i] < queueFamilyProperties[i].queueCount)
                    && (queueFamilyProperties[i].queueFlags & type))
                {
                    auto& queueCreateInfo = queueCreateInfos[i];
                    auto& queuePriorityCreateInfo = queuePriorityCreateInfos[i];

                    if (queueCreateInfo.pNext == nullptr ||
                        queuePriorityCreateInfo.globalPriority == globalPriority)
                    {
                        queuePriorityCreateInfos[i].globalPriority = globalPriority;
                        queueCreateInfos[i].pNext = &queuePriorityCreateInfos[i];
                        queueCreateInfos[i].queueCount++;

                        auto& queueInfo = CreateQueueInfo(type, globalPriority);
                        queueInfo.offset = queueFamilyNextOffset[i];
                        queueInfo.familyIndex = i;

                        queueCreateInfos[i].queueCount = queueFamilyProperties[i].queueCount;
                        if (globalPriority == VkQueueGlobalPriorityEXT::VK_QUEUE_GLOBAL_PRIORITY_REALTIME_EXT
                            || globalPriority == VkQueueGlobalPriorityEXT::VK_QUEUE_GLOBAL_PRIORITY_HIGH_EXT) {
                            queueInfo.offset = queueCreateInfos[i].queueCount - 1;
                        }

                        queueFamilyNextOffset[i]++;
                        return;
                    } else {
                        LOG("Queue family %d already assigned priority %d and trying to assign priority %d\n",
                        i, queuePriorityCreateInfo.globalPriority, globalPriority);
                    }
                }
            }
            LOG("Unable to add queue of type %d and priority %d\n", type, globalPriority);
            exit(-1);
        };

        // As the Queue with graphics capabilitiies also has compute, reserve it for graphics before its selected for compute
        for (auto priority : graphicPriorities) {
            addQueue(VK_QUEUE_GRAPHICS_BIT, priority);
        }
        for (auto priority : computePriorities) {
            addQueue(VK_QUEUE_COMPUTE_BIT, priority);
        }

		// Create logical device
		VkDeviceCreateInfo deviceCreateInfo = {};
		deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		deviceCreateInfo.queueCreateInfoCount = queueCreateInfos.size();
		deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
		VK_CHECK_RESULT(vkCreateDevice(m_physicalDevice, &deviceCreateInfo, nullptr, &m_device));

        auto getQueue = [&](QueueInfo& queueInfo) {
            vkGetDeviceQueue(m_device, queueInfo.familyIndex, queueInfo.offset, &queueInfo.queue);
        };

        LOG("Graphic queues : %zu\n", m_graphicQueues.size());
        for (auto& item: m_graphicQueues) {
            auto& queueInfo = item.second;
            getQueue(queueInfo);
            LOG(" [%p] familyIndex %d, priority %d\n", queueInfo.queue, queueInfo.familyIndex, queueInfo.priority);
        }

        LOG("Compute queues : %zu\n", m_computeQueues.size());
        for (auto& item: m_computeQueues) {
            auto& queueInfo = item.second;
            getQueue(queueInfo);
            LOG(" [%p] familyIndex %d, priority %d\n", queueInfo.queue, queueInfo.familyIndex, queueInfo.priority);
        }
    }

    ~Base() {
		vkDestroyDevice(m_device, nullptr);
		vkDestroyInstance(m_instance, nullptr);
    }
};
