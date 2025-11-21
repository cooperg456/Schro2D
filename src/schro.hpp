#pragma once

//	std lib
#include <vector>

//	glfw3
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

//	vulkan memory allocator
#define VMA_VULKAN_VERSION 1003000
#include <vma/vk_mem_alloc.h>

//	vulkan api
#include <vulkan/vulkan.hpp>



//	struct to hold engine configuration variables
struct Schro2DConfig {
	uint32_t width;		//	glfw window width (pixels)
	uint32_t height;	//	glfw window height (pixels)
	double scale;		//	multiplier for sim resolution
	bool validation;	//	toggle for including "VK_LAYER_KHRONOS_validation" layer
	bool portability;	//	toggle for including "VK_KHR_portability_subset" extension and flags
};

//	struct to hold per frame data [unused]
struct FrameObjects {
	vk::Fence fence;				//	fence
	vk::Semaphore imageSem;			//	image semaphore
	vk::Semaphore renderSem;		//	render semaphore
	vk::CommandBuffer cmdBuffer;	//	command buffer
	vk::CommandPool cmdPool;		//	command pool
};

//	schrodinger equation solver using vulkan
class Schro2D {
public:
	//	initialize engine components
	Schro2D(Schro2DConfig config);
	//	cleanup vulkan/glfw
	~Schro2D();
	//	starts program
	void run();

private:
	//	##  initializers for engine components
	//	######################################################

	//	initializes window
	void createWindow();
	//	initializes instance and surface
	void createInstance();
	//	sets physical device
	void setPhysicalDevice();
	//	sets queue family
	void setQueueFamily();
	//	initializes device, queue
	void createDevice();
	//	initializes allocator
	void createAllocator();
	//	initializes swap chain, images, image views, fence, semaphores
	void createSwapChain();
	//	initializes stuff for compute pipeline
	void createComputePipeline();

	//	##  simulation loop functions
	//	######################################################

	//	update window
	void draw(uint8_t frameIdx);

	//	##	context configuration and components:
	//	######################################################

	const uint32_t viewportWidth_;		//	glfw window width (pixels)
	const uint32_t viewportHeight_;		//	glfw window height (pixels)
	const double simScale_;				//	multiplier for sim resolution
	const bool validationEnabled_;		//	toggle for including "VK_LAYER_KHRONOS_validation" layer
	const bool portabilityEnabled_;		//	toggle for including "VK_KHR_portability_subset" extension and flags
	
	vk::Instance instance_{};				//	instance
	vk::PhysicalDevice physicalDevice_{};	//	physical device
	uint32_t queueFamily_ = 0xFFFFFFFF;		//	queue family
	vk::Device device_{};					//	device
	vk::Queue queue_{};						//	queue
	VmaAllocator allocator_{};				//	allocator

	GLFWwindow* window_ {};			//	window
	VkSurfaceKHR surface_{};		//	surface
	vk::SwapchainKHR swapchain_{};	//	swap chain

	std::vector<vk::Fence> fences_{};					//	fence
	std::vector<vk::Semaphore> imageSemaphores_{};		//	image semaphore
	std::vector<vk::Semaphore> renderSemaphores_{};		//	render semaphore
	std::vector<vk::Image> images_{};					//	swap chain images
	std::vector<vk::ImageView> imageViews_{};			//	swap chain image views
	std::vector<vk::CommandBuffer> commandBuffers_{};	//	command buffers
	std::vector<vk::CommandPool> commandPools_{};		//	command pools

	vk::ShaderModule shaderModule_{};					//	shader module
	vk::DescriptorSetLayout descriptorSetLayout_{};		//	descriptor set layout
	vk::PipelineLayout pipelineLayout_{};				//	pipeline layout
	vk::Pipeline computePipeline_{};					//	compute pipeline
	vk::DescriptorPool descriptorPool_{};				//	descriptor pool
	std::vector<vk::DescriptorSet> descriptorSets_{};	//	descriptor sets
};