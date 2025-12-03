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



//	struct to hold per frame data
struct FrameData {
	vk::Fence fence;				//	fence
	vk::Semaphore imageSem;			//	image semaphore
	vk::Semaphore renderSem;		//	render semaphore
	vk::CommandBuffer cmdBuffer;	//	command buffer
	vk::CommandPool cmdPool;		//	command pool
	vk::Image image;				//	swapchain image
	vk::ImageView view;				//	swapchain image view
};

//	schrodinger equation solver using vulkan
class Schro2D {
public:
	//	initialize engine components
	Schro2D(uint32_t width, uint32_t height, double scale);
	//	cleanup vulkan/glfw
	~Schro2D();
	//	starts program
	void run();

private:
	//	##  initializers for engine components
	//	######################################################

	//	initializes window using glfw 3.4
	void createWindow();
	//	initializes vulkan instance with required extensions and glfw window surface
	void createInstance();
	//	selects supported gpu from list of hardware options
	void setPhysicalDevice();
	//	selects supported queue family on gpu
	void setQueueFamily();
	//	initializes logical device and queue
	void createDevice();
	//	initializes vma allocator
	void createAllocator();
	//	initializes swapchain with images and sync structures
	void createSwapChain();
	//	initializes compute pipeline, storage buffers, and descriptor sets
	void createComputePipeline();

	//	##  simulation loop functions
	//	######################################################

	//	update window
	void draw(uint8_t frameIdx);

	//	##	context configuration and components:
	//	######################################################

	//	simulation config
	const uint32_t viewportWidth_;		//	glfw window width (pixels)
	const uint32_t viewportHeight_;		//	glfw window height (pixels)
	const double simScale_;				//	multiplier for sim resolution
	
	//	engine components
	vk::Instance instance_{};				//	instance
	vk::PhysicalDevice physicalDevice_{};	//	physical device
	uint32_t queueFamily_ = UINT32_MAX;		//	queue family
	vk::Device device_{};					//	device
	vk::Queue queue_{};						//	queue
	VmaAllocator allocator_{};				//	allocator

	//	render components
	GLFWwindow* window_ {};					//	window
	VkSurfaceKHR surface_{};				//	surface
	vk::SwapchainKHR swapchain_{};			//	swapchain
	std::vector<FrameData> frameData_{};	//	per frame data structures

	//	compute pipeline
	vk::ShaderModule shaderModule_{};					//	shader module
	vk::DescriptorSetLayout descriptorSetLayout_{};		//	descriptor set layout
	vk::PipelineLayout pipelineLayout_{};				//	pipeline layout
	vk::Pipeline computePipeline_{};					//	compute pipeline
	vk::DescriptorPool descriptorPool_{};				//	descriptor pool
	std::vector<vk::DescriptorSet> descriptorSets_{};	//	descriptor sets

	//	compute storage
	std::vector<vk::Buffer> psiBuffer_{};	//	buffers (ssbo) containing wave function values
};