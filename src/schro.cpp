//	std lib
#include <iostream>
#include <fstream>
#include <complex>
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

//	debug and compatability options
#ifdef DEBUG
	constexpr bool VALIDATION_ENABLED = true;	//	toggle for including "VK_LAYER_KHRONOS_validation" layer
#else
	constexpr bool VALIDATION_ENABLED = false;	//	toggle for including "VK_LAYER_KHRONOS_validation" layer
#endif

#ifdef __APPLE__
	constexpr bool PORTABILITY_ENABLED = true;	//	toggle for including "VK_KHR_portability_subset" extension and related flags
#else
	constexpr bool PORTABILITY_ENABLED = false;	//	toggle for including "VK_KHR_portability_subset" extension and related flags
#endif







//	---------------------------------------------------
//	--	constructor and destructor:
//	---------------------------------------------------
#pragma region constructor;



Schro2D::Schro2D(uint32_t width, uint32_t height, double scale)
: viewportWidth_(width), viewportHeight_(height), simScale_(scale) {
	if (VALIDATION_ENABLED) {
		std::cout << "Schro2D: 'VK_LAYER_KHRONOS_validation' enabled" << std::endl;
	}
	if (PORTABILITY_ENABLED) {
		std::cout << "Schro2D: 'VK_KHR_portability_subset' enabled" << std::endl;
	}
	//	create engine components
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

	for (size_t i = 0; i < psiBuffer_.size(); i++) {
		if (psiBuffer_[i]) vmaDestroyBuffer(allocator_, psiBuffer_[i], psiAlloc_[i]);
	}
	if (vBuffer_) vmaDestroyBuffer(allocator_, vBuffer_, vAlloc_);

	if (descriptorPool_) device_.destroyDescriptorPool(descriptorPool_);
	if (computePipeline_) device_.destroyPipeline(computePipeline_);
	if (pipelineLayout_) device_.destroyPipelineLayout(pipelineLayout_);
	if (descriptorSetLayout_) device_.destroyDescriptorSetLayout(descriptorSetLayout_);
	if (shaderModule_) device_.destroyShaderModule(shaderModule_);

	for (auto frame : frameData_) {
		device_.destroyImageView(frame.view);
		device_.freeCommandBuffers(frame.cmdPool, frame.cmdBuffer);
		device_.destroyCommandPool(frame.cmdPool);
		device_.destroySemaphore(frame.renderSem);
		device_.destroySemaphore(frame.imageSem);
		device_.destroyFence(frame.fence);
	}
	if (swapchain_) device_.destroySwapchainKHR(swapchain_);
	if (allocator_) vmaDestroyAllocator(allocator_);
	if (device_) device_.destroy(); 
	if (surface_) vkDestroySurfaceKHR(instance_, surface_, nullptr);
	if (instance_) instance_.destroy(); 
	
	glfwDestroyWindow(window_);
	glfwTerminate();
}




//	---------------------------------------------------
//	--	initializers for engine components:
//	---------------------------------------------------
#pragma region initializers;



void Schro2D::createWindow() {
	if (!glfwInit()) throw std::runtime_error("Failed to initialize glfw");
	
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);	// tell glfw not to create OpenGL context. necessary for Vulkan
	glfwWindowHint(GLFW_RESIZABLE, false);

	window_ = glfwCreateWindow(viewportWidth_, viewportHeight_, "Schro2D : Vulkan", nullptr, nullptr);
}



void Schro2D::createInstance() {
	vk::ApplicationInfo appInfo{ 
		"Schro2D", VK_MAKE_VERSION(0, 1, 0), 			//	application
		"Minimal_Vk_GLFW", VK_MAKE_VERSION(0, 1, 0), 	//	engine
		VK_API_VERSION_1_3 								//	vulkan
	};

	vk::InstanceCreateFlags flags{};
	std::vector<const char*> layers{};
	std::vector<const char*> extensions{};
	
	//	glfw extensions and flags
	uint32_t glfwExtensionsCount = 0;
	const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionsCount);
	for (uint32_t i = 0; i < glfwExtensionsCount; i++) extensions.emplace_back(glfwExtensions[i]);
	extensions.emplace_back(vk::EXTSwapchainColorSpaceExtensionName);
	
	//	portability extensions and flags
	if (PORTABILITY_ENABLED) {
		flags |= vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
		extensions.emplace_back(vk::KHRPortabilityEnumerationExtensionName);
		extensions.emplace_back(vk::KHRGetPhysicalDeviceProperties2ExtensionName);
	}

	//  validation layers
	if (VALIDATION_ENABLED) layers.emplace_back("VK_LAYER_KHRONOS_validation");

	//	create components
	vk::InstanceCreateInfo instanceCreateInfo{ flags, &appInfo, layers, extensions };
	instance_ = vk::createInstance(instanceCreateInfo);

	VkResult result = glfwCreateWindowSurface(instance_, window_, nullptr, &surface_);
	if (result != VK_SUCCESS) throw std::runtime_error(string_VkResult(result));
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
		if ((queueFamilyProperties[queueFamily_].queueFlags & requiredFlags) == requiredFlags) return;
	}
	throw std::runtime_error("No suitable queue family was found");
}



void Schro2D::createDevice() {
	float queuePriority = 1.0f;
	vk::DeviceQueueCreateInfo deviceQueueCreateInfo = { vk::DeviceQueueCreateFlags(), queueFamily_, 1, &queuePriority };

	std::vector<const char*> deviceExtensions{};
	deviceExtensions.emplace_back(vk::KHRSwapchainExtensionName);

	//	portability device extension
	if (PORTABILITY_ENABLED) deviceExtensions.emplace_back("VK_KHR_portability_subset");

	//	enable sync 2 feature
	vk::PhysicalDeviceVulkan13Features deviceFeatures13{};
	deviceFeatures13.synchronization2 = true;
	vk::PhysicalDeviceFeatures2 deviceFeatures2{};
	deviceFeatures2.pNext = deviceFeatures13;

	//	create components
	vk::DeviceCreateInfo deviceCreateInfo{
		vk::DeviceCreateFlags(), 
		deviceQueueCreateInfo, 
		nullptr, 
		deviceExtensions, 
		nullptr, &deviceFeatures2 
	};
	
	device_ = physicalDevice_.createDevice(deviceCreateInfo);

	queue_ = device_.getQueue(queueFamily_, 0);
}



void Schro2D::createAllocator() {
	VmaAllocatorCreateInfo allocatorCreateInfo{};
	allocatorCreateInfo.flags = VmaAllocatorCreateFlags();
	allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_3;
	allocatorCreateInfo.physicalDevice = static_cast<VkPhysicalDevice>(physicalDevice_);
	allocatorCreateInfo.device = static_cast<VkDevice>(device_);
	allocatorCreateInfo.instance = static_cast<VkInstance>(instance_);

	VkResult result = vmaCreateAllocator(&allocatorCreateInfo, &allocator_);
	if (result != VK_SUCCESS) throw std::runtime_error(string_VkResult(result));
}



void Schro2D::createSwapChain() {
	vk::SurfaceCapabilitiesKHR swapChainCapabilities = physicalDevice_.getSurfaceCapabilitiesKHR(surface_);

	std::vector<vk::SurfaceFormatKHR> surfaceFormats = physicalDevice_.getSurfaceFormatsKHR(surface_);
	vk::SurfaceFormatKHR surfaceFormat;
	for (size_t i = 0; i < surfaceFormats.size(); i++) {
		if (surfaceFormats[i] == vk::SurfaceFormatKHR(vk::Format::eR8G8B8A8Srgb, vk::ColorSpaceKHR::eSrgbNonlinear)) {
			surfaceFormat = surfaceFormats[i];	//	standard format, colorspace
		}
		if (surfaceFormats[i] == vk::SurfaceFormatKHR(vk::Format::eR16G16B16A16Sfloat, vk::ColorSpaceKHR::eHdr10HlgEXT)) {
			surfaceFormat = surfaceFormats[i];	//	hdr (if supported)
			break;
		}
	}

	vk::SwapchainCreateInfoKHR swapChainCreateInfo{
		vk::SwapchainCreateFlagsKHR(),
		surface_,
		2,
		surfaceFormat.format,
		surfaceFormat.colorSpace,
		{ 
			static_cast<uint32_t>(viewportWidth_ * simScale_), 
			static_cast<uint32_t>(viewportHeight_ * simScale_) 
		},
		1,
		vk::ImageUsageFlagBits::eColorAttachment | 
		vk::ImageUsageFlagBits::eStorage,
		vk::SharingMode::eExclusive,
		queueFamily_,
		nullptr,
		swapChainCapabilities.currentTransform,
		vk::CompositeAlphaFlagBitsKHR::eOpaque,
		vk::PresentModeKHR::eFifo,
		false	
	};

	swapchain_ = device_.createSwapchainKHR(swapChainCreateInfo);

	auto images = device_.getSwapchainImagesKHR(swapchain_);
	frameData_.resize(images.size());
	for (size_t i = 0; i < images.size(); i++) {
		//	image structures
		frameData_[i].image = images[i];

		vk::ImageViewCreateInfo imageViewCreateInfo{
			vk::ImageViewCreateFlags(),
			images[i],
			vk::ImageViewType::e2D,
			surfaceFormat.format,
			{
				vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity,
				vk::ComponentSwizzle::eIdentity, vk::ComponentSwizzle::eIdentity
			},
			{ 
				vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 
			}
		};

		frameData_[i].view = device_.createImageView(imageViewCreateInfo);

		//	sync structures
		frameData_[i].fence = device_.createFence({vk::FenceCreateFlagBits::eSignaled});
		frameData_[i].imageSem = device_.createSemaphore(vk::SemaphoreCreateInfo());
		frameData_[i].renderSem = device_.createSemaphore(vk::SemaphoreCreateInfo());

		//	cmd structures
		frameData_[i].cmdPool = device_.createCommandPool({vk::CommandPoolCreateFlagBits::eResetCommandBuffer, queueFamily_});

		vk::CommandBufferAllocateInfo commandBufferAllocateInfo{ frameData_[i].cmdPool, vk::CommandBufferLevel::ePrimary, 1 };

		frameData_[i].cmdBuffer = device_.allocateCommandBuffers(commandBufferAllocateInfo).front();
	}
}



void Schro2D::createComputePipeline() {
	// read SPIR-V file to create shader module
    std::ifstream shaderFile("bin/schro.spv", std::ios::binary | std::ios::ate);
    if (!shaderFile.is_open()) {
        throw std::runtime_error("Failed to read shader file");
    }
    size_t shaderSize = (size_t)shaderFile.tellg();
	std::vector<char> shaderData(shaderSize);

    shaderFile.seekg(0);
    shaderFile.read(shaderData.data(), shaderSize);
	shaderFile.close();

    vk::ShaderModuleCreateInfo shaderModuleCreateInfo{
		vk::ShaderModuleCreateFlags(),
		(uint32_t)shaderSize, reinterpret_cast<const uint32_t*>(shaderData.data()),
	};

    shaderModule_ = device_.createShaderModule(shaderModuleCreateInfo);

	VmaAllocationCreateInfo gpuAllocInfo{};
	gpuAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
	gpuAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT; 

	psiBuffer_.resize(3);
	psiAlloc_.resize(3);
	vk::BufferCreateInfo storageBufferCreateInfo{
		vk::BufferCreateFlags(), 
		sizeof(float) * (uint32_t)(2 * viewportWidth_ * viewportHeight_ * simScale_ * simScale_), 
		vk::BufferUsageFlagBits::eStorageBuffer | 
		vk::BufferUsageFlagBits::eTransferDst, 
		vk::SharingMode::eExclusive
	};
	for (size_t i = 0; i < 2; i++) {
		vmaCreateBuffer(allocator_, storageBufferCreateInfo, &gpuAllocInfo, 
			reinterpret_cast<VkBuffer*>(&psiBuffer_[i]), &psiAlloc_[i], nullptr
		);
	}
	vmaCreateBuffer(allocator_, storageBufferCreateInfo, &gpuAllocInfo, 
			reinterpret_cast<VkBuffer*>(&psiBuffer_[2]), &psiAlloc_[2], nullptr
		);
	vmaCreateBuffer(
		allocator_, storageBufferCreateInfo, &gpuAllocInfo, 
		reinterpret_cast<VkBuffer*>(&vBuffer_), &vAlloc_, nullptr
	);

	// boring vulkan boilerplate
	std::vector<vk::DescriptorSetLayoutBinding> descriptorSetLayoutBindings{
		{ 0, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute },
		{ 1, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute },
		{ 2, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute },
		{ 3, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute },
		{ 4, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute }
	};

	vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{
		vk::DescriptorSetLayoutCreateFlags(), descriptorSetLayoutBindings
	};

	descriptorSetLayout_ = device_.createDescriptorSetLayout(descriptorSetLayoutCreateInfo);

	vk::PushConstantRange pushConstantRange{ vk::ShaderStageFlagBits::eCompute, 0, sizeof(float) + sizeof(uint32_t) };

	vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo{
		vk::PipelineLayoutCreateFlags(), descriptorSetLayout_, pushConstantRange
	};

	pipelineLayout_ = device_.createPipelineLayout(pipelineLayoutCreateInfo);

	vk::PipelineShaderStageCreateInfo pipelineShaderStageCreateInfo{
		vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eCompute, shaderModule_, "main"
	};

	vk::ComputePipelineCreateInfo computePipelineCreateInfo{
		vk::PipelineCreateFlags(), pipelineShaderStageCreateInfo, pipelineLayout_
	};

	computePipeline_ = device_.createComputePipeline(nullptr, computePipelineCreateInfo).value;

	std::vector<vk::DescriptorPoolSize> descriptorPoolSizes{
		{ vk::DescriptorType::eStorageImage, 2 }, { vk::DescriptorType::eStorageBuffer, 8 }
	};

	vk::DescriptorPoolCreateInfo descriptorPoolCreateInfo{ vk::DescriptorPoolCreateFlags(), 2, descriptorPoolSizes };

	descriptorPool_ = device_.createDescriptorPool(descriptorPoolCreateInfo);

	std::vector<vk::DescriptorSetLayout> layouts(2, descriptorSetLayout_);

	vk::DescriptorSetAllocateInfo descriptorSetAllocateInfo{ descriptorPool_, layouts };

	descriptorSets_ = device_.allocateDescriptorSets(descriptorSetAllocateInfo);

	std::vector<vk::DescriptorBufferInfo> descriptorBufferInfos{
		{ psiBuffer_[0], 0, vk::WholeSize }, { psiBuffer_[1], 0, vk::WholeSize }, 
		{ vBuffer_, 0, vk::WholeSize }, { psiBuffer_[2], 0, vk::WholeSize }
	};

	for (size_t i = 0; i < 2; i++) {
		vk::DescriptorImageInfo descriptorImageInfo{ nullptr, frameData_[i].view, vk::ImageLayout::eGeneral };

		std::vector<vk::WriteDescriptorSet> writeDescriptorSets{ 
			{ descriptorSets_[i], 0, 0, 1, vk::DescriptorType::eStorageImage, &descriptorImageInfo, nullptr, nullptr },
			{ descriptorSets_[i], 1, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &descriptorBufferInfos[i], nullptr },
			{ descriptorSets_[i], 2, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &descriptorBufferInfos[i ^ 1], nullptr },
			{ descriptorSets_[i], 3, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &descriptorBufferInfos[2], nullptr },
			{ descriptorSets_[i], 4, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &descriptorBufferInfos[3], nullptr }
		};

		device_.updateDescriptorSets(writeDescriptorSets, nullptr);
	}
}



//	---------------------------------------------------
//	--	simulation loop function:
//	---------------------------------------------------
#pragma region sim loop;



void Schro2D::draw(uint8_t frameIdx, float pushConst) {
	vk::Result waitResult = device_.waitForFences(frameData_[frameIdx].fence, true, 0xFFFFFFFF);
	if (waitResult != vk::Result::eSuccess) throw std::runtime_error(vk::to_string(waitResult));
	device_.resetFences(frameData_[frameIdx].fence);

	uint32_t imageIdx;
	vk::Result acquireResult = device_.acquireNextImageKHR(swapchain_, 0xFFFFFFFF, frameData_[frameIdx].imageSem, nullptr, &imageIdx);
	if (acquireResult != vk::Result::eSuccess) throw std::runtime_error(vk::to_string(acquireResult));

	frameData_[frameIdx].cmdBuffer.reset();
	frameData_[frameIdx].cmdBuffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

	//	---------------------------------------------------
	//	--	begin cmd:
	//	---------------------------------------------------

	vk::ImageSubresourceRange imageSubresourceRange{
		vk::ImageAspectFlagBits::eColor, 0, vk::RemainingMipLevels, 0, vk::RemainingArrayLayers
	};

    vk::ImageMemoryBarrier2 imageBarrier{
		vk::PipelineStageFlagBits2::eAllCommands, vk::AccessFlagBits2::eMemoryWrite,
		vk::PipelineStageFlagBits2::eAllCommands, vk::AccessFlagBits2::eMemoryRead,
		vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral,
		queueFamily_, queueFamily_, frameData_[imageIdx].image, imageSubresourceRange
	};

	vk::DependencyInfo dependencyInfo{ vk::DependencyFlags(), nullptr, nullptr, imageBarrier };

    frameData_[frameIdx].cmdBuffer.pipelineBarrier2(dependencyInfo);

	//	do schrodinger equation
	frameData_[frameIdx].cmdBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, computePipeline_);
	frameData_[frameIdx].cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipelineLayout_, 0, descriptorSets_[imageIdx], nullptr);

	for (uint32_t stage = 0; stage < 3; stage++) {
		frameData_[frameIdx].cmdBuffer.pushConstants(pipelineLayout_, vk::ShaderStageFlagBits::eCompute, 0, sizeof(float), &pushConst);
		frameData_[frameIdx].cmdBuffer.pushConstants(pipelineLayout_, vk::ShaderStageFlagBits::eCompute, sizeof(float), sizeof(uint32_t), &stage);
		frameData_[frameIdx].cmdBuffer.dispatch((simScale_ * viewportWidth_ + 31) / 32, (simScale_ * viewportHeight_ + 31) / 32, 1);
	}

    vk::ImageMemoryBarrier2 imageBarrier2{
		vk::PipelineStageFlagBits2::eAllCommands, vk::AccessFlagBits2::eMemoryWrite,
		vk::PipelineStageFlagBits2::eAllCommands, vk::AccessFlagBits2::eMemoryRead,
		vk::ImageLayout::eGeneral, vk::ImageLayout::ePresentSrcKHR,
		queueFamily_, queueFamily_, frameData_[imageIdx].image, imageSubresourceRange
	};

	vk::DependencyInfo dependencyInfo2{ vk::DependencyFlags(), nullptr, nullptr, imageBarrier2 };

    frameData_[frameIdx].cmdBuffer.pipelineBarrier2(dependencyInfo2);

	//	---------------------------------------------------
	//	--	end cmd:
	//	---------------------------------------------------
	
	frameData_[frameIdx].cmdBuffer.end();

	vk::SemaphoreSubmitInfo waitSemaphoreInfo{
		frameData_[frameIdx].imageSem, 1, vk::PipelineStageFlagBits2::eColorAttachmentOutput, 0
	};

	vk::SemaphoreSubmitInfo submitSemaphoreInfo{
		frameData_[frameIdx].renderSem, 1, vk::PipelineStageFlagBits2::eColorAttachmentOutput, 0
	};

	vk::CommandBufferSubmitInfo commandBufferSubmitInfo{ frameData_[frameIdx].cmdBuffer, 0 };

	vk::SubmitInfo2 submitInfo{ vk::SubmitFlagBits(), waitSemaphoreInfo, commandBufferSubmitInfo, submitSemaphoreInfo };

	queue_.submit2(submitInfo, frameData_[frameIdx].fence);

	vk::PresentInfoKHR presentInfo{ frameData_[frameIdx].renderSem, swapchain_, imageIdx };

	vk::Result presentResult = queue_.presentKHR(presentInfo);
	if (presentResult != vk::Result::eSuccess) throw std::runtime_error(vk::to_string(presentResult));
}



void Schro2D::run(std::vector<std::vector<std::complex<float>>>& wavefn,
		std::vector<std::vector<std::complex<float>>>& potential, float pushConst) {
	//	prep and load gpu arrays
	std::vector<std::complex<float>> psi{};
	for (const auto& row : wavefn) for (const auto& coord : row) psi.emplace_back(coord);
	vmaCopyMemoryToAllocation(allocator_, psi.data(), psiAlloc_[0], 0, sizeof(std::complex<float>) * psi.size());
	vmaCopyMemoryToAllocation(allocator_, psi.data(), psiAlloc_[1], 0, sizeof(std::complex<float>) * psi.size());

	std::vector<std::complex<float>> v{};
	for (const auto& row : potential) for (const auto& coord : row) v.emplace_back(coord);
	vmaCopyMemoryToAllocation(allocator_, v.data(), vAlloc_, 0, sizeof(std::complex<float>) * v.size());

	//	render loop
	uint8_t frameIdx = 0;
	int frames = 0;
	while (!glfwWindowShouldClose(window_)) {
		glfwPollEvents();
		draw(frameIdx, pushConst);
		frameIdx ^= 1;

		if (frames % 100 == 0) {
			std::vector<std::complex<float>> psiHost(wavefn.size() * wavefn[0].size());
			vmaCopyAllocationToMemory(allocator_, psiAlloc_[0], 0, psiHost.data(), sizeof(std::complex<float>) * psiHost.size());

			// Calculate normalization
			float norm = 0;
			for (const auto& val : psiHost) {
				norm += std::norm(val) / (viewportWidth_ * viewportHeight_ * simScale_ * simScale_);
			}

			std::cout << frames << ",\t" << norm << "\n";
		}

		frames++;
	}
}