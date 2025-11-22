//	std lib
#include <iostream>
#include <fstream>
#include <vector>

//	glfw3
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

//	vulkan memory allocator
#define VMA_IMPLEMENTATION
#define VMA_VULKAN_VERSION 1003000
#include <vma/vk_mem_alloc.h>

//	vulkan api
#include <vulkan/vulkan.hpp>
#include <vulkan/vk_enum_string_helper.h>

//	header
#include "schro.hpp"



//	##  constructors and destructors
//	######################################################

Schro2D::Schro2D(Schro2DConfig config)
: viewportWidth_(config.width), viewportHeight_(config.height), simScale_(config.scale),
portabilityEnabled_(config.portability), validationEnabled_(config.validation) {
	createWindow();
	createInstance();
	setPhysicalDevice();
	setQueueFamily();
	createDevice();
	createAllocator();
	createSwapChain();
	createComputePipeline();
}

Schro2D::~Schro2D() {
	device_.waitIdle();

	if (descriptorPool_) {
		device_.destroyDescriptorPool(descriptorPool_);
	}
	if (computePipeline_) {
		device_.destroyPipeline(computePipeline_);
	}
	if (pipelineLayout_) {
		device_.destroyPipelineLayout(pipelineLayout_);
	}
	if (descriptorSetLayout_) {
		device_.destroyDescriptorSetLayout(descriptorSetLayout_);
	}
	if (shaderModule_) {
		device_.destroyShaderModule(shaderModule_);
	}
	for (auto image : imageData_) {
		if (image.isSwapchainImage) {
			device_.destroyImageView(image.view);
		}
		else {
			//	...
		}
	}
	for (auto frame : frameData_) {
		device_.freeCommandBuffers(frame.cmdPool, frame.cmdBuffer);
		device_.destroyCommandPool(frame.cmdPool);
		device_.destroySemaphore(frame.renderSem);
		device_.destroySemaphore(frame.imageSem);
		device_.destroyFence(frame.fence);
	}
	if (swapchain_) {
		device_.destroySwapchainKHR(swapchain_);
	}
	if (allocator_) { 
		vmaDestroyAllocator(allocator_); 
	}
	if (device_) { 
		device_.destroy(); 
	}
	if (surface_) { 
		vkDestroySurfaceKHR(instance_, surface_, nullptr); 
	}
	if (instance_) { 
		instance_.destroy(); 
	}
	glfwDestroyWindow(window_);
	glfwTerminate();
}

//	##  initializers for engine components
//	######################################################

void Schro2D::createWindow() {
	if (!glfwInit()) {
		throw std::runtime_error("Failed to initialize glfw");
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);	// tell glfw not to create OpenGL context. necessary for Vulkan
	glfwWindowHint(GLFW_RESIZABLE, false);

	window_ = glfwCreateWindow(viewportWidth_, viewportHeight_, "Schro2D : Vulkan", nullptr, nullptr);
}

void Schro2D::createInstance() {
	vk::ApplicationInfo appInfo = {
		"Schro2D",					//	application
		VK_MAKE_VERSION(0, 1, 0),
		"Minimal_Vk_GLFW",			//	engine
		VK_MAKE_VERSION(0, 1, 0),
		VK_API_VERSION_1_3			// vulkan
	};
	
	//	glfw extensions and flags
	vk::InstanceCreateFlags flags = {};
	uint32_t glfwExtensionsCount = 0;
	const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionsCount);
	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionsCount);
	extensions.emplace_back(vk::EXTSwapchainColorSpaceExtensionName);
	
	//	portability extensions and flags
	if (portabilityEnabled_) {
		flags |= vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
		extensions.emplace_back(vk::KHRPortabilityEnumerationExtensionName);
		extensions.emplace_back(vk::KHRGetPhysicalDeviceProperties2ExtensionName);
	}

	//  validation layers
	std::vector<const char*> layers = {};
	if (validationEnabled_) {
		layers.emplace_back("VK_LAYER_KHRONOS_validation");
	}

	vk::InstanceCreateInfo instanceCreateInfo = { flags, &appInfo, layers, extensions };

	instance_ = vk::createInstance(instanceCreateInfo);

	VkResult result = glfwCreateWindowSurface(instance_, window_, nullptr, &surface_);
	if (result != VK_SUCCESS) {
		throw std::runtime_error(string_VkResult(result));
	}
}

void Schro2D::setPhysicalDevice() {
	//  TODO?:  improve device selection logic. 
	//          fine for single gpu systems, but could cause problems if integrated graphics is front
	std::vector<vk::PhysicalDevice> physicalDevices = instance_.enumeratePhysicalDevices();
	if (physicalDevices.front()) { 
		physicalDevice_ = physicalDevices.front();
		return;
	}
	throw std::runtime_error("No suitable physical device was found");
}

void Schro2D::setQueueFamily() {
	//  TODO?:  improve queue family selection logic
	//          choosing the first supported family is probably not optimal
	constexpr vk::QueueFlags requiredFlags = vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute;
	std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice_.getQueueFamilyProperties();
	for (queueFamily_ = 0; queueFamily_ < (uint32_t)queueFamilyProperties.size(); queueFamily_++) {
		if ((queueFamilyProperties[queueFamily_].queueFlags & requiredFlags) == requiredFlags) { 
			return;
		}
	}
	throw std::runtime_error("No suitable queue family was found");
}

void Schro2D::createDevice() {
	float queuePriority = 1.0f;
	vk::DeviceQueueCreateInfo deviceQueueCreateInfo = { 
		vk::DeviceQueueCreateFlags(), 
		queueFamily_, 
		1, 
		&queuePriority
	};
	std::vector<const char*> deviceExtensions = { 
		vk::KHRSwapchainExtensionName,
	};

	//	portability device extension
	if (portabilityEnabled_) {
		deviceExtensions.push_back("VK_KHR_portability_subset");
	}

	vk::PhysicalDeviceVulkan13Features deviceFeatures13 = {};
	deviceFeatures13.synchronization2 = true;

	vk::PhysicalDeviceFeatures2 deviceFeatures = {};
	deviceFeatures.pNext = deviceFeatures13;

	vk::DeviceCreateInfo deviceCreateInfo = {
		vk::DeviceCreateFlags(), 
		1, 
		&deviceQueueCreateInfo, 
		0, 
		nullptr, 
		static_cast<uint32_t>(deviceExtensions.size()), 
		deviceExtensions.data(),
		nullptr,
		deviceFeatures
	};
	
	device_ = physicalDevice_.createDevice(deviceCreateInfo);
	queue_ = device_.getQueue(queueFamily_, 0);
}

void Schro2D::createAllocator() {
	VmaAllocatorCreateInfo allocatorCreateInfo = {};
	allocatorCreateInfo.flags = VmaAllocatorCreateFlags();
	allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_3;
	allocatorCreateInfo.physicalDevice = static_cast<VkPhysicalDevice>(physicalDevice_);
	allocatorCreateInfo.device = static_cast<VkDevice>(device_);
	allocatorCreateInfo.instance = static_cast<VkInstance>(instance_);

	VkResult result = vmaCreateAllocator(&allocatorCreateInfo, &allocator_);
	if (result != VK_SUCCESS) {
		throw std::runtime_error(string_VkResult(result));
	}
}

void Schro2D::createSwapChain() {
	vk::SurfaceCapabilitiesKHR swapChainCapabilities = physicalDevice_.getSurfaceCapabilitiesKHR(surface_);

	std::vector<vk::SurfaceFormatKHR> surfaceFormats = physicalDevice_.getSurfaceFormatsKHR(surface_);
	vk::SurfaceFormatKHR surfaceFormat;
	for (size_t i = 0; i < surfaceFormats.size(); i++) {
		//	standard format, colorspace
		if (surfaceFormats[i].format == vk::Format::eR8G8B8A8Srgb 
			&& surfaceFormats[i].colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
			surfaceFormat = surfaceFormats[i];
		}
		//	hdr (if supported)
		if (surfaceFormats[i].format == vk::Format::eR16G16B16A16Sfloat 
			&& surfaceFormats[i].colorSpace == vk::ColorSpaceKHR::eHdr10HlgEXT) {
			surfaceFormat = surfaceFormats[i];
			break;
		}
	}

	vk::PresentModeKHR presentMode = vk::PresentModeKHR::eFifo;

	VkExtent2D extent2D = { static_cast<uint32_t>(viewportWidth_ * simScale_), static_cast<uint32_t>(viewportHeight_ * simScale_) };

	vk::ImageUsageFlags imageUsageFlags = vk::ImageUsageFlagBits::eColorAttachment 
		| vk::ImageUsageFlagBits::eTransferDst
		| vk::ImageUsageFlagBits::eStorage;

	vk::SwapchainCreateInfoKHR swapChainCreateInfo = {
		vk::SwapchainCreateFlagsKHR(),
		surface_,
		swapChainCapabilities.minImageCount,
		surfaceFormat.format,
		surfaceFormat.colorSpace,
		extent2D,
		1,
		imageUsageFlags,
		vk::SharingMode::eExclusive,
		0,
		nullptr,
		swapChainCapabilities.currentTransform,
		vk::CompositeAlphaFlagBitsKHR::eOpaque,
		presentMode,
		false,
		nullptr
	};

	swapchain_ = device_.createSwapchainKHR(swapChainCreateInfo);
	auto images = device_.getSwapchainImagesKHR(swapchain_);
	frameData_.resize(images.size());
	imageData_.resize(images.size());

	vk::FenceCreateInfo fenceCreateInfo = { vk::FenceCreateFlagBits::eSignaled };

	vk::CommandPoolCreateInfo commandPoolCreateInfo = { vk::CommandPoolCreateFlagBits::eResetCommandBuffer, queueFamily_ };

	for (size_t i = 0; i < images.size(); i++) {
		//	image structures
		imageData_[i].image = images[i];
		imageData_[i].isSwapchainImage = true;

		vk::ImageViewCreateInfo imageViewCreateInfo = {
			vk::ImageViewCreateFlags(),
			images[i],
			vk::ImageViewType::e2D,
			surfaceFormat.format,
			{
				vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity,
				vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity
			},
			{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 }
		};
		imageData_[i].view = device_.createImageView(imageViewCreateInfo);

		//	sync structures
		frameData_[i].fence = device_.createFence(fenceCreateInfo);
		frameData_[i].imageSem = device_.createSemaphore(vk::SemaphoreCreateInfo());
		frameData_[i].renderSem = device_.createSemaphore(vk::SemaphoreCreateInfo());

		//	cmd structures
		frameData_[i].cmdPool = device_.createCommandPool(commandPoolCreateInfo);

		vk::CommandBufferAllocateInfo commandBufferAllocateInfo = {
			frameData_[i].cmdPool,
			vk::CommandBufferLevel::ePrimary,
			1
		};

		frameData_[i].cmdBuffer = device_.allocateCommandBuffers(commandBufferAllocateInfo).front();
	}
}

void Schro2D::createComputePipeline() {
	// read SPIR-V file to create shader module
    std::ifstream shaderFile("bin/shaders/grad.comp.spv", std::ios::binary | std::ios::ate);
    if (!shaderFile.is_open()) {
        throw std::runtime_error("Failed to read shader file");
    }
    size_t shaderSize = (size_t)shaderFile.tellg();
	std::vector<char> shaderData(shaderSize);

    shaderFile.seekg(0);
    shaderFile.read(shaderData.data(), shaderSize);
	shaderFile.close();

    vk::ShaderModuleCreateInfo shaderModuleCreateInfo = {
		vk::ShaderModuleCreateFlags(),
		(uint32_t)shaderSize,
		reinterpret_cast<const uint32_t*>(shaderData.data()),
	};

    shaderModule_ = device_.createShaderModule(shaderModuleCreateInfo);

	// boring vulkan boilerplate
	std::vector<vk::DescriptorSetLayoutBinding> descriptorSetLayoutBindings;
	descriptorSetLayoutBindings.emplace_back(0, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute);

	vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {
		vk::DescriptorSetLayoutCreateFlags(),
		(uint32_t)descriptorSetLayoutBindings.size(), descriptorSetLayoutBindings.data()
	};

	descriptorSetLayout_ = device_.createDescriptorSetLayout(descriptorSetLayoutCreateInfo);

	vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
		vk::PipelineLayoutCreateFlags(),
		descriptorSetLayout_
	};

	pipelineLayout_ = device_.createPipelineLayout(pipelineLayoutCreateInfo);

	vk::PipelineShaderStageCreateInfo pipelineShaderStageCreateInfo = {
		vk::PipelineShaderStageCreateFlags(),
		vk::ShaderStageFlagBits::eCompute,
		shaderModule_,
		"main"
	};

	vk::ComputePipelineCreateInfo computePipelineCreateInfo = {
		vk::PipelineCreateFlags(),
		pipelineShaderStageCreateInfo,
		pipelineLayout_
	};

	computePipeline_ = device_.createComputePipeline(nullptr, computePipelineCreateInfo).value;

	vk::DescriptorPoolSize descriptorPoolSize = {
		vk::DescriptorType::eStorageImage,
		(uint32_t)frameData_.size()
	};

	vk::DescriptorPoolCreateInfo descriptorPoolCreateInfo = {
		vk::DescriptorPoolCreateFlags(),
		(uint32_t)frameData_.size(), 
		descriptorPoolSize
	};

	descriptorPool_ = device_.createDescriptorPool(descriptorPoolCreateInfo);

	std::vector<vk::DescriptorSetLayout> layouts(frameData_.size(), descriptorSetLayout_);

	vk::DescriptorSetAllocateInfo descriptorSetAllocateInfo = {
		descriptorPool_,
		(uint32_t)layouts.size(),
		layouts.data()
	};

	descriptorSets_ = device_.allocateDescriptorSets(descriptorSetAllocateInfo);

	for (size_t i = 0; i < frameData_.size(); i++) {
		vk::DescriptorImageInfo descriptorImageInfo = { nullptr, imageData_[i].view, vk::ImageLayout::eGeneral };

		vk::WriteDescriptorSet writeDescriptorSet = { 
			descriptorSets_[i],
			0, 0, 1,
			vk::DescriptorType::eStorageImage,
			&descriptorImageInfo
		};

		device_.updateDescriptorSets(writeDescriptorSet, nullptr);
	}
}

//	##  simulation loop functions
//	######################################################

void Schro2D::draw(uint8_t frameIdx) {
	vk::Result waitResult = device_.waitForFences(frameData_[frameIdx].fence, true, 0xFFFFFFFF);
	if (waitResult != vk::Result::eSuccess) {
		throw std::runtime_error(vk::to_string(waitResult));
	}
	device_.resetFences(frameData_[frameIdx].fence);

	uint32_t imageIdx;
	vk::Result acquireResult = device_.acquireNextImageKHR(swapchain_, 0xFFFFFFFF, frameData_[frameIdx].imageSem, nullptr, &imageIdx);
	if (acquireResult != vk::Result::eSuccess) {
		throw std::runtime_error(vk::to_string(acquireResult));
	}

	frameData_[frameIdx].cmdBuffer.reset();
	frameData_[frameIdx].cmdBuffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

	//	##  CMD BEGIN
	//	######################################################

	vk::ImageSubresourceRange imageSubresourceRange = {
		vk::ImageAspectFlagBits::eColor,
		0, vk::RemainingMipLevels,
		0, vk::RemainingArrayLayers
	};

    vk::ImageMemoryBarrier2 imageBarrier = {
		vk::PipelineStageFlagBits2::eAllCommands,
		vk::AccessFlagBits2::eMemoryWrite,
		vk::PipelineStageFlagBits2::eAllCommands,
		vk::AccessFlagBits2::eMemoryRead,
		vk::ImageLayout::eUndefined,
		vk::ImageLayout::eGeneral,
		queueFamily_,
		queueFamily_,
		imageData_[imageIdx].image,
		imageSubresourceRange
	};

	vk::DependencyInfo dependencyInfo = { vk::DependencyFlags(), nullptr, nullptr, imageBarrier };

    frameData_[frameIdx].cmdBuffer.pipelineBarrier2(dependencyInfo);

	// vk::ClearColorValue clearValue = { 0.5f, 0.0f, 0.5f, 1.0f };
	// commandBuffers_[frameIdx].clearColorImage(images_[swapchainIndex], vk::ImageLayout::eGeneral, clearValue, imageSubresourceRange);

	frameData_[frameIdx].cmdBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, computePipeline_);
	frameData_[frameIdx].cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipelineLayout_, 0, descriptorSets_[imageIdx], nullptr);
	frameData_[frameIdx].cmdBuffer.dispatch((simScale_ * viewportWidth_ + 31) / 32, (simScale_ * viewportHeight_ + 31) / 32, 1);

    imageBarrier = {
		vk::PipelineStageFlagBits2::eAllCommands,
		vk::AccessFlagBits2::eMemoryWrite,
		vk::PipelineStageFlagBits2::eAllCommands,
		vk::AccessFlagBits2::eMemoryRead,
		vk::ImageLayout::eGeneral,
		vk::ImageLayout::ePresentSrcKHR,
		queueFamily_,
		queueFamily_,
		imageData_[imageIdx].image,
		imageSubresourceRange
	};

    frameData_[frameIdx].cmdBuffer.pipelineBarrier2(dependencyInfo);

	//	##  CMD END
	//	######################################################
	
	frameData_[frameIdx].cmdBuffer.end();

	vk::SemaphoreSubmitInfo waitSemaphoreInfo = {
		frameData_[frameIdx].imageSem, 1,
		vk::PipelineStageFlagBits2::eColorAttachmentOutput, 0
	};

	vk::SemaphoreSubmitInfo submitSemaphoreInfo = {
		frameData_[frameIdx].renderSem, 1,
		vk::PipelineStageFlagBits2::eColorAttachmentOutput, 0
	};

	vk::CommandBufferSubmitInfo commandBufferSubmitInfo = { frameData_[frameIdx].cmdBuffer, 0 };

	vk::SubmitInfo2 submitInfo = { vk::SubmitFlagBits(), waitSemaphoreInfo, commandBufferSubmitInfo, submitSemaphoreInfo };

	queue_.submit2(submitInfo, frameData_[frameIdx].fence);

	vk::PresentInfoKHR presentInfo = { frameData_[frameIdx].renderSem, swapchain_, imageIdx };

	vk::Result presentResult = queue_.presentKHR(presentInfo);
	if (presentResult != vk::Result::eSuccess) {
		throw std::runtime_error(vk::to_string(presentResult));
	}
}

//	##	run simulation loop
//	######################################################

void Schro2D::run() {
	uint8_t frameIdx = 0;

	while (!glfwWindowShouldClose(window_)) {
		glfwPollEvents();
		draw(frameIdx);
		frameIdx = (frameIdx + 1) % ((uint8_t)frameData_.size());
	}
}