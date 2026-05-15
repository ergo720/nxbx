// SPDX-License-Identifier: GPL-3.0-only
// SPDX-FileCopyrightText: 2026 ergo720

#include "vulkan/vulkan.h"
#include "init.hpp"
#include "logger.hpp"
#include <filesystem>
#include <fstream>
#include <vector>
#include <bit>
#include <set>
#include <cinttypes>
#include <optional>
#include <cstring>

#define MODULE_NAME vulkan

#define NUM_COMPUTE_PIPELINES 1
#define PIPELINE_PUSHER_IDX 0


class Vulkan::Impl
{
public:
	void init(const std::string &nxbx_dir, uint8_t *ram_ptr, uint64_t ram_size);
	void deinit();
	void setValidationLayers(uint32_t enable);

private:
	struct QueueFamilyIndices {
		std::optional<uint32_t> m_compute;
		bool isComplete() { return m_compute.has_value(); }
	};

	void createInstance();
	void selectPhysicalDevice();
	void createLogicalDevice();
	void createComputeDescriptorSetLayout();
	void createNv2aPusherComputePipeline(const std::string &shader_root_path);
	bool areValidationLayersSupported();
	VkPhysicalDeviceFeatures2 enableDeviceFeatures();
	bool canUseDevice(VkPhysicalDevice device);
	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
	bool checkDeviceExtensionSupport(VkPhysicalDevice device);
	VkShaderModule createShaderModule(const std::string_view shader_path);
	void importCpuRamBuffer(uint8_t *ram_ptr, uint64_t ram_size);
	std::optional<uint32_t> findMemoryType(uint32_t required, uint32_t mask) const;
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type,
		const VkDebugUtilsMessengerCallbackDataEXT* cb, void *opaque)
	{
		static_assert(std::to_underlying(VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) == 0x0000001);
		static_assert(std::to_underlying(VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) == 0x0000010);
		static_assert(std::to_underlying(VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) == 0x00000100);
		static_assert(std::to_underlying(VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) == 0x00001000);

		auto lv = static_cast<log_lv>(std::countr_zero(static_cast<uint32_t>(severity)));
		logger<log_module::vulkan, true>(lv, cb->pMessage);

		return VK_FALSE;
	}

	const std::vector<const char *> m_dbg_validation = {
		"VK_LAYER_KHRONOS_validation", // required to verify correct usage of the Vulkan API
	};
	const std::vector<const char *> m_dbg_ext = {
		"VK_EXT_debug_utils" // required for EXT debug functions
	};
	const std::vector<const char *> m_device_ext = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME, // required for presenting images to the screen in Vulkan
		"VK_EXT_external_memory_host", // required for importing the host ram buffer to Vulkan
		"VK_KHR_buffer_device_address" // required for accessing buffers with 64-bit buffer device addresses
	};
	VkInstance m_instance;
	VkDebugUtilsMessengerEXT m_debug_messenger;
	VkPhysicalDevice m_pdev;
	VkDevice m_ldev;
	VkQueue m_queue_compute;
	VkDescriptorSetLayout m_compute_desc_set_layout;
	VkPipelineLayout m_compute_pipeline_layout;
	VkPipeline m_compute_pipeline[NUM_COMPUTE_PIPELINES];
	VkPhysicalDeviceMemoryProperties m_mem_props;
	std::vector<VkBuffer> m_buffers;
	std::vector<VkDeviceMemory> m_dev_mems;
	uint32_t m_validation_en;
	uint64_t m_ram_ptr_alignment;
	// EXT functions might not necessarly be available in core vk, so they must be loaded at runtime
	PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT;
	PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT;
	PFN_vkGetMemoryHostPointerPropertiesEXT vkGetMemoryHostPointerPropertiesEXT;
};

void Vulkan::Impl::init(const std::string &nxbx_dir, uint8_t *ram_ptr, uint64_t ram_size)
{
	createInstance();
	selectPhysicalDevice();
	createLogicalDevice();
	importCpuRamBuffer(ram_ptr, ram_size);
	std::string shader_root_path(nxbx_dir + "/shaders");
	createNv2aPusherComputePipeline(shader_root_path);
}

void Vulkan::Impl::deinit()
{
	vkDestroyPipeline(m_ldev, m_compute_pipeline[PIPELINE_PUSHER_IDX], nullptr);
	vkDestroyPipelineLayout(m_ldev, m_compute_pipeline_layout, nullptr);

	vkDestroyDescriptorSetLayout(m_ldev, m_compute_desc_set_layout, nullptr);

	for (size_t i = 0; i < m_buffers.size(); ++i) {
		vkDestroyBuffer(m_ldev, m_buffers[i], nullptr);
		vkFreeMemory(m_ldev, m_dev_mems[i], nullptr);
	}

	vkDestroyDevice(m_ldev, nullptr);

	if (m_validation_en) {
		vkDestroyDebugUtilsMessengerEXT(m_instance, m_debug_messenger, nullptr);
	}

	vkDestroyInstance(m_instance, nullptr);
}

void Vulkan::Impl::setValidationLayers(uint32_t enable)
{
	m_validation_en = enable;
}

void Vulkan::Impl::createInstance()
{
	if (auto func = vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceVersion"); !func) {
		throw std::runtime_error(highestlv2str("nxbx requires at least Vulkan version 1.1"));
	}

	if (m_validation_en && !areValidationLayersSupported()) {
		throw std::runtime_error(highestlv2str("Validation layers requested, but not available"));
	}

	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "nxbx";
	appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0); // dummy, as nxbx doesn't have a version number
	appInfo.pEngineName = "No Engine";
	appInfo.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_1;

	VkInstanceCreateInfo create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	create_info.pApplicationInfo = &appInfo;

	if (m_validation_en) {
		create_info.enabledLayerCount = static_cast<uint32_t>(m_dbg_validation.size());
		create_info.ppEnabledLayerNames = m_dbg_validation.data();
		create_info.enabledExtensionCount = static_cast<uint32_t>(m_dbg_ext.size());;
		create_info.ppEnabledExtensionNames = m_dbg_ext.data();

		// Setup a logging callback for the validation messages
		VkDebugUtilsMessengerCreateInfoEXT dbg_create_info{};
		dbg_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		dbg_create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		dbg_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		dbg_create_info.pfnUserCallback = debugCallback;
		create_info.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&dbg_create_info;

		if (auto ret = vkCreateInstance(&create_info, nullptr, &m_instance); ret != VK_SUCCESS) {
			throw std::runtime_error(highestlv2str("Failed to create instance"));
		}

		if (vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT"); !vkCreateDebugUtilsMessengerEXT) {
			throw std::runtime_error(highestlv2str("vkCreateDebugUtilsMessengerEXT not available"));
		}
		if (vkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT"); !vkDestroyDebugUtilsMessengerEXT) {
			throw std::runtime_error(highestlv2str("vkDestroyDebugUtilsMessengerEXT not available"));
		}

		if (vkCreateDebugUtilsMessengerEXT(m_instance, &dbg_create_info, nullptr, &m_debug_messenger) != VK_SUCCESS) {
			throw std::runtime_error(highestlv2str("Failed to set up debug messenger"));
		}

		return;
	}

	create_info.enabledExtensionCount = 0;
	create_info.ppEnabledExtensionNames = nullptr;
	create_info.enabledLayerCount = 0;
	create_info.pNext = nullptr;

	if (vkCreateInstance(&create_info, nullptr, &m_instance) != VK_SUCCESS) {
		throw std::runtime_error(highestlv2str("Failed to create instance"));
	}
}

void Vulkan::Impl::selectPhysicalDevice()
{
	uint32_t device_count = 0;
	vkEnumeratePhysicalDevices(m_instance, &device_count, nullptr);

	if (device_count == 0) {
		throw std::runtime_error(highestlv2str("Failed to find GPUs with Vulkan support"));
	}

	std::vector<VkPhysicalDevice> devices(device_count);
	vkEnumeratePhysicalDevices(m_instance, &device_count, devices.data());
	m_pdev = VK_NULL_HANDLE;

	for (const auto& device : devices) {
		if (canUseDevice(device)) {
			break;
		}
	}

	if (m_pdev == VK_NULL_HANDLE) {
		throw std::runtime_error(highestlv2str("Failed to find a suitable GPU"));
	}
}

void Vulkan::Impl::createLogicalDevice()
{
	QueueFamilyIndices indices = findQueueFamilies(m_pdev);

	std::vector<VkDeviceQueueCreateInfo> queue_ci_vec;
	std::set<uint32_t> queue_families = { indices.m_compute.value() };

	float priority = 1.0f;
	for (uint32_t family : queue_families) {
		VkDeviceQueueCreateInfo queue_ci{};
		queue_ci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_ci.queueFamilyIndex = family;
		queue_ci.queueCount = 1;
		queue_ci.pQueuePriorities = &priority;
		queue_ci_vec.push_back(queue_ci);
	}

	auto dev_features = enableDeviceFeatures();
	VkDeviceCreateInfo dev_ci{};
	dev_ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	dev_ci.queueCreateInfoCount = static_cast<uint32_t>(queue_ci_vec.size());
	dev_ci.pQueueCreateInfos = queue_ci_vec.data();
	dev_ci.pNext = &dev_features;
	dev_ci.enabledExtensionCount = static_cast<uint32_t>(m_device_ext.size());
	dev_ci.ppEnabledExtensionNames = m_device_ext.data();

	if (m_validation_en) {
		dev_ci.enabledLayerCount = static_cast<uint32_t>(m_dbg_validation.size());
		dev_ci.ppEnabledLayerNames = m_dbg_validation.data();
	}
	else {
		dev_ci.enabledLayerCount = 0;
	}

	if (vkCreateDevice(m_pdev, &dev_ci, nullptr, &m_ldev) != VK_SUCCESS) {
		throw std::runtime_error(highestlv2str("Failed to create logical device"));
	}

	if (vkGetMemoryHostPointerPropertiesEXT = (PFN_vkGetMemoryHostPointerPropertiesEXT)vkGetDeviceProcAddr(m_ldev, "vkGetMemoryHostPointerPropertiesEXT"); !vkGetMemoryHostPointerPropertiesEXT) {
		throw std::runtime_error(highestlv2str("vkGetMemoryHostPointerPropertiesEXT not available"));
	}

	vkGetDeviceQueue(m_ldev, indices.m_compute.value(), 0, &m_queue_compute);
}

void Vulkan::Impl::createComputeDescriptorSetLayout()
{
	VkDescriptorSetLayoutBinding desc_lb;
	desc_lb.binding = 0;
	desc_lb.descriptorCount = 1;
	desc_lb.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	desc_lb.pImmutableSamplers = nullptr;
	desc_lb.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	VkDescriptorSetLayoutCreateInfo desc_li{};
	desc_li.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	desc_li.bindingCount = 1;
	desc_li.pBindings = &desc_lb;

	if (vkCreateDescriptorSetLayout(m_ldev, &desc_li, nullptr, &m_compute_desc_set_layout) != VK_SUCCESS) {
		throw std::runtime_error(highestlv2str("Failed to create compute descriptor set layout"));
	}
}

void Vulkan::Impl::createNv2aPusherComputePipeline(const std::string &shader_root_path)
{
	createComputeDescriptorSetLayout();
	VkShaderModule pb_pusher_mod = createShaderModule(shader_root_path + "/pb_pusher.comp.spv");

	VkPipelineShaderStageCreateInfo compute_shader_si{};
	compute_shader_si.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	compute_shader_si.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	compute_shader_si.module = pb_pusher_mod;
	compute_shader_si.pName = "main";

	VkPipelineLayoutCreateInfo pipeline_li{};
	pipeline_li.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeline_li.setLayoutCount = 1;
	pipeline_li.pSetLayouts = &m_compute_desc_set_layout;

	if (vkCreatePipelineLayout(m_ldev, &pipeline_li, nullptr, &m_compute_pipeline_layout) != VK_SUCCESS) {
		throw std::runtime_error(highestlv2str("Failed to create compute pipeline layout"));
	}

	VkComputePipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipelineInfo.layout = m_compute_pipeline_layout;
	pipelineInfo.stage = compute_shader_si;

	if (vkCreateComputePipelines(m_ldev, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_compute_pipeline[PIPELINE_PUSHER_IDX]) != VK_SUCCESS) {
		throw std::runtime_error(highestlv2str("Failed to create compute pipeline"));
	}

	vkDestroyShaderModule(m_ldev, pb_pusher_mod, nullptr);
}

bool Vulkan::Impl::areValidationLayersSupported()
{
	uint32_t layer_count;
	vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
	std::vector<VkLayerProperties> supported_layers(layer_count);
	vkEnumerateInstanceLayerProperties(&layer_count, supported_layers.data());

	for (const char* layerName : m_dbg_validation) {
		bool layer_found = false;

		for (const auto& layer_properties : supported_layers) {
			if (std::strcmp(layerName, layer_properties.layerName) == 0) {
				layer_found = true;
				break;
			}
		}

		if (layer_found) {
			return true;
		}
	}

	return false;
}

VkPhysicalDeviceFeatures2 Vulkan::Impl::enableDeviceFeatures()
{
	VkPhysicalDeviceDescriptorIndexingFeatures desc_idx_features{};
	desc_idx_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;

	VkPhysicalDeviceBufferDeviceAddressFeatures buff_dev_addr_features{};
	buff_dev_addr_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
	buff_dev_addr_features.pNext = &desc_idx_features;

	VkPhysicalDeviceFeatures2 dev_features{};
	dev_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	dev_features.pNext = &buff_dev_addr_features;

	vkGetPhysicalDeviceFeatures2(m_pdev, &dev_features);
	if (buff_dev_addr_features.bufferDeviceAddress == VK_FALSE) {
		throw std::runtime_error(highestlv2str("bufferDeviceAddress feature not supported but required"));
	}

	return dev_features;
}

bool Vulkan::Impl::canUseDevice(VkPhysicalDevice device)
{
	QueueFamilyIndices indices = findQueueFamilies(device);
	bool ext_supported = checkDeviceExtensionSupport(device);

	VkPhysicalDeviceExternalMemoryHostPropertiesEXT dev_prop_ext{};
	dev_prop_ext.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_HOST_PROPERTIES_EXT;

	VkPhysicalDeviceProperties2 dev_prop{};
	dev_prop.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	dev_prop.pNext = &dev_prop_ext;

	vkGetPhysicalDeviceProperties2(device, &dev_prop);
	uint64_t ram_ptr_alignment = dev_prop_ext.minImportedHostPointerAlignment;

	if (indices.isComplete() && ext_supported && (((64 * 1024) % ram_ptr_alignment)) == 0) { // 64 KiB -> alignment used by cpu for ram buffer
		// We are going to use this device, so log some info about it
		m_pdev = device;

		const auto &&lambda = [](VkPhysicalDeviceType type)
			{
				switch (type)
				{
				case VK_PHYSICAL_DEVICE_TYPE_OTHER:
					return "Other";

				case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
					return "Integrated Gpu";

				case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
					return "Discrete Gpu";

				case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
					return "Virtual Gpu";

				case VK_PHYSICAL_DEVICE_TYPE_CPU:
					return "Cpu";

				default:
					return "Unknown";
				}
			};

		VkPhysicalDeviceMemoryProperties2 mem_prop{};
		mem_prop.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
		vkGetPhysicalDeviceMemoryProperties2(m_pdev, &mem_prop);
		std::memcpy(&m_mem_props, &mem_prop.memoryProperties, sizeof(mem_prop.memoryProperties));
		m_ram_ptr_alignment = ram_ptr_alignment;

		logger_en(info, "Vulkan version: %u.%u.%u.%u\nDriver version: %u\nVendorID: 0x%08" PRIX32 "\nDeviceID: 0x%08" PRIX32 "\nDevice: %s\nName: %s",
			VK_API_VERSION_VARIANT(dev_prop.properties.apiVersion),
			VK_API_VERSION_MAJOR(dev_prop.properties.apiVersion),
			VK_API_VERSION_MINOR(dev_prop.properties.apiVersion),
			VK_API_VERSION_PATCH(dev_prop.properties.apiVersion),
			dev_prop.properties.driverVersion,
			dev_prop.properties.vendorID,
			dev_prop.properties.deviceID,
			lambda(dev_prop.properties.deviceType),
			dev_prop.properties.deviceName
		);

		return true;
	}

	return false;
}

Vulkan::Impl::QueueFamilyIndices Vulkan::Impl::findQueueFamilies(VkPhysicalDevice device)
{
	QueueFamilyIndices indices;

	uint32_t queue_family_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);
	std::vector<VkQueueFamilyProperties> queueFamilies(queue_family_count);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queueFamilies.data());

	uint32_t i = 0;
	for (const auto& queueFamily : queueFamilies) {
		if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) { // compute family required
			indices.m_compute = i;
		}

		if (indices.isComplete()) {
			break;
		}

		++i;
	}

	return indices;
}

bool Vulkan::Impl::checkDeviceExtensionSupport(VkPhysicalDevice device)
{
	uint32_t ext_num;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &ext_num, nullptr);
	std::vector<VkExtensionProperties> available_ext(ext_num);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &ext_num, available_ext.data());

	std::set<std::string> required_ext(m_device_ext.begin(), m_device_ext.end());

	for (const auto& ext : available_ext) {
		required_ext.erase(ext.extensionName);
	}

	return required_ext.empty();
}

VkShaderModule Vulkan::Impl::createShaderModule(const std::string_view shader_path)
{
	std::ifstream file(shader_path.data(), std::ios_base::ate | std::ios_base::binary);

	if (!file.is_open()) {
		throw std::runtime_error(highestlv2str(("Failed to open shaders/" + std::filesystem::path(shader_path).filename().string()).c_str()));
	}

	auto file_size = file.tellg();
	std::vector<char> code(file_size);
	file.seekg(0);
	file.read(code.data(), file_size);
	if (!file.good()) {
		throw std::runtime_error(highestlv2str(("Failed to read shaders/" + std::filesystem::path(shader_path).filename().string()).c_str()));
	}

	VkShaderModuleCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	ci.codeSize = code.size();
	ci.pCode = reinterpret_cast<const uint32_t*>(code.data());

	VkShaderModule shader_mod;
	if (vkCreateShaderModule(m_ldev, &ci, nullptr, &shader_mod) != VK_SUCCESS) {
		throw std::runtime_error(highestlv2str(("Failed to create module for shaders/" + std::filesystem::path(shader_path).filename().string()).c_str()));
	}

	return shader_mod;
}

void Vulkan::Impl::importCpuRamBuffer(uint8_t *ram_ptr, uint64_t ram_size)
{
	VkMemoryHostPointerPropertiesEXT ptr_prop{};
	ptr_prop.sType = VK_STRUCTURE_TYPE_MEMORY_HOST_POINTER_PROPERTIES_EXT;
	if (vkGetMemoryHostPointerPropertiesEXT(m_ldev, VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT, ram_ptr, &ptr_prop) != VK_SUCCESS) {
		throw std::runtime_error(highestlv2str("Ram pointer is not importable"));
	}

	VkExternalMemoryBufferCreateInfo ext_ci{};
	ext_ci.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO;
	ext_ci.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT;

	VkBufferCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	ci.size = (ram_size + 64 * 1024 - 1) & ~(64 * 1024 - 1); // 64 KiB -> alignment used by cpu for ram buffer
	ci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	ci.pNext = &ext_ci;

	VkBuffer buffer;
	if (vkCreateBuffer(m_ldev, &ci, nullptr, &buffer) != VK_SUCCESS) {
		throw std::runtime_error(highestlv2str("Failed to create ram buffer"));
	}

	VkMemoryRequirements reqs;
	vkGetBufferMemoryRequirements(m_ldev, buffer, &reqs);

	// Fix from parallel-rdp: "Weird workaround for latest AMD Windows drivers which sets memoryTypeBits to 0 when using the external handle type"
	if (!reqs.memoryTypeBits) {
		reqs.memoryTypeBits = ~0u;
	}

	auto plain_reqs = reqs;
	reqs.memoryTypeBits &= ptr_prop.memoryTypeBits;

	if (reqs.memoryTypeBits == 0) {
		vkDestroyBuffer(m_ldev, buffer, nullptr);
		throw std::runtime_error(highestlv2str("No compatible host pointer types are available"));
	}

	auto memory_type = findMemoryType(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, reqs.memoryTypeBits);

	if (memory_type == std::nullopt) {
		// Fix from parallel-rdp:
		// "Weird workaround for Intel Windows where the only memory type is DEVICE_LOCAL
		// with no HOST_VISIBLE (!?!?!).
		// However, it appears to work just fine to allocate with other memory types as well ...
		// Oh well."

		// Ignore host_pointer_props.
		reqs = plain_reqs;
		memory_type = findMemoryType(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, reqs.memoryTypeBits);
	}

	if (memory_type == std::nullopt) {
		vkDestroyBuffer(m_ldev, buffer, nullptr);
		throw std::runtime_error(highestlv2str("Failed to find memory type"));
	}

	VkMemoryAllocateInfo alloc_info{};
	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.allocationSize = (ram_size + 64 * 1024 - 1) & ~(64 * 1024 - 1); // 64 KiB -> alignment used by cpu for ram buffer
	alloc_info.memoryTypeIndex = memory_type.value();

	VkMemoryAllocateFlagsInfo alloc_fi{};
	alloc_fi.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
	alloc_fi.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
	alloc_info.pNext = &alloc_fi;

	VkImportMemoryHostPointerInfoEXT import{};
	import.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT;
	import.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT;
	import.pHostPointer = ram_ptr;
	import.pNext = alloc_info.pNext;
	alloc_info.pNext = &import;

	VkDeviceMemory memory;
	if (vkAllocateMemory(m_ldev, &alloc_info, nullptr, &memory) != VK_SUCCESS) {
		vkDestroyBuffer(m_ldev, buffer, nullptr);
		throw std::runtime_error(highestlv2str("Failed to import ram buffer"));
	}

	void *dummy;
	if (vkMapMemory(m_ldev, memory, 0, VK_WHOLE_SIZE, 0, &dummy) != VK_SUCCESS) {
		vkDestroyBuffer(m_ldev, buffer, nullptr);
		vkFreeMemory(m_ldev, memory, nullptr);
		throw std::runtime_error(highestlv2str("Failed to map ram buffer"));
	}

	if (vkBindBufferMemory(m_ldev, buffer, memory, 0) != VK_SUCCESS) {
		vkDestroyBuffer(m_ldev, buffer, nullptr);
		vkFreeMemory(m_ldev, memory, nullptr);
		throw std::runtime_error(highestlv2str("Failed to bind ram buffer"));
	}

	m_buffers.push_back(buffer);
	m_dev_mems.push_back(memory);
}

std::optional<uint32_t> Vulkan::Impl::findMemoryType(uint32_t required, uint32_t mask) const
{
	for (uint32_t i = 0; i < m_mem_props.memoryTypeCount; ++i) {
		if ((1u << i) & mask) {
			uint32_t flags = m_mem_props.memoryTypes[i].propertyFlags;
			if ((flags & required) == required) {
				return i;
			}
		}
	}

	return std::nullopt;
}

/** Public interface implementation **/
void Vulkan::init(const std::string &nxbx_dir, uint8_t *ram_ptr, uint64_t ram_size)
{
	m_impl->init(nxbx_dir, ram_ptr, ram_size);
}

void Vulkan::deinit()
{
	m_impl->deinit();
}

void Vulkan::setValidationLayers(uint32_t enable)
{
	m_impl->setValidationLayers(enable);
}

Vulkan::Vulkan() : m_impl{std::make_unique<Vulkan::Impl>()} {}
Vulkan::~Vulkan() {}
