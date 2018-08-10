// NAME
//   vkrtlib.h
// VERSION
//    0.1
// SYNOPSIS
//    Vulkan runtime library -- vkrtlib -- is a GPGPU engine based on Vulkan.
//    It eats SPIR-V compute shaders and executes them without any graphical
//    context, much like OpenCL. The interface is very abstract and allows you
//    to get to work with your shaders as quickly as possible. With the current
//    state of compute shaders, you should be able to achieve ~OpenCL 1.2
//    feature parity.
// AUTHOR
//    Marcio Machado Pereira

#include <vector>
#include <vulkan/vulkan.h>

namespace vkrtl {

enum Error {
    VKRTL_ERROR_CREATE_REPORT_CALLBACK,
    VKRTL_ERROR_CREATE_INSTANCE,
    VKRTL_ERROR_DEVICES,
    VKRTL_ERROR_COMPUTE_QUEUE,
    VKRTL_ERROR_SUBMIT_QUEUE,
    VKRTL_ERROR_DESCRIPTOR,
    VKRTL_ERROR_SHADER,
    VKRTL_ERROR_PIPELINE,
    VKRTL_ERROR_COMMAND_POOL,
    VKRTL_ERROR_COMMAND_BUFFER,
    VKRTL_ERROR_CREATE_BUFFER,
    VKRTL_ERROR_MAP,
    VKRTL_ERROR_MALLOC
};

// Specifies a storage buffer descriptor as the Resource Type.
enum ResourceType { STORAGE_BUFFER = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER };

enum ModeOptions { VKRTL_none, VKRTL_verbose, VKRTL_profile, VKRTL_all };

class Kernel;
class Program;
class Arguments;
class CommandBuffer;
class Device;

/*
 * The Object class is responsible for the creation and destruction
 * of the Vulkan instance object.
 */
class Object {
  private:
    // In order to use Vulkan we must create an instance
    VkInstance instance;

    // The logical devices basically allows us to interact
    // with physical devices.
    std::vector<Device> devices;

    // Debug report callbacks give more detailed feedback
    // on the application’s use of Vulkan when events occur.
    VkDebugReportCallbackEXT debugReportCallback;

  public:
    Object(enum ModeOptions mode = VKRTL_none);
    ~Object();
    std::vector<Device> &getDevices();
    Device &getDevice();
    VkInstance &getInstance();
};

/*
 * Device objects represent logical connections to physical devices.
 * Each device exposes a number of queue families each having one or
 * more queues. All queues in a queue family support the same operations.
 */
class Device {
  protected:
    // The physical device is some device on the system that supports usage
    // of Vulkan. Often, it is simply a graphics card that supports Vulkan.
    VkPhysicalDevice physicalDevice;
    VkPhysicalDeviceProperties physicalDeviceProperties;
    VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties;

    // Then we have the logical device VkDevice, which basically allows
    // us to interact with the physical device.
    VkDevice device;

    // A queue supporting compute operations.
    VkQueue queue;

    // The command buffer is used to record commands, that will be submitted to a queue.
    CommandBuffer *implicitCommandBuffer;

    // index of mappable memory type
    int memoryTypeMappable = -1;

    // index of local memory type
    int memoryTypeLocal = -1;

    // index of the queue family that support compute operations
    int computeQueueFamily = -1;

  public:
    Device(VkPhysicalDevice physicalDevice);
    void destroy();
    void showProperties();
    void submit(VkCommandBuffer commandBuffer);
    void wait();
    const char *getName();
    uint32_t getVendorId();
};

/*
 * Command buffers are objects used to record commands which can be
 * subsequently submitted to a device queue for execution.
 */
class CommandBuffer : protected Device {
  private:
    // The command buffer is used to record commands,
    // that will be submitted to a queue.
    VkCommandBuffer commandBuffer;

    // To allocate such command buffers, we use a command pool.
    VkCommandPool commandPool;

    void sharedConstructor();

  public:
    CommandBuffer(Device &device);
    CommandBuffer(Device &device, Kernel &kernel, Arguments &arguments);
    void destroy();
    operator VkCommandBuffer();
    void begin();
    void barrier();

    // The number of local work groups in each of the x, y, and z dimensions
    // is passed in the x, y, and z parameters, respectively. A valid compute
    // pipeline must be bound to the command buffer at the
    // VK_PIPELINE_BIND_POINT_COMPUTE binding point.
    // When the command is executed by the device, a global work group of
    // x * y * z local work groups begins executing the shader contained
    // in the bound pipeline.
    void dispatch(int x = 1, int y = 1, int z = 1);
    void end();
};

/*
 * Buffers represent linear arrays of data which are used for various
 * purposes by binding them to a graphics or compute pipeline via descriptor
 * sets or via certain commands, or by directly specifying them as parameters
 * to certain commands.
 */
class Buffer : protected Device {
  private:
    VkDeviceMemory memory;
    VkBuffer buffer;

  public:
    Buffer(Device &device, size_t byteSize, bool mappable = false);
    void enqueueCopy(Buffer src, Buffer dst, size_t byteSize, VkCommandBuffer commandBuffer);
    void inload(void *hostPtr);
    void offload(void *hostPtr);
    operator VkBuffer();
    void destroy();
    void unmap();
    void *map();
};


class Program : protected Device {
  protected:
    // Shader modules are represented by VkShaderModule handles.
    // Shader modules contain shader code and one or more entry points.
    // Shaders are selected from a shader module by specifying an entry
    // point as part of pipeline creation. The stages of a pipeline can
    // use shaders that come from different modules. The shader code
    // defining a shader module must be in the SPIR-V format.
    VkShaderModule shaderModule;

  public:
    Program(Device &device, const char *fileName);
    Program(Device &device, uint32_t *data);
    void destroy();
};


class Kernel : protected Program {
  protected:
	// The pipeline layout is used by a pipeline to access the descriptor sets
	// It defines interface (without binding any actual data) between the shader
    // stages used by the pipeline and the shader resources
	// A pipeline layout can be shared among multiple pipelines as long as their
    // interfaces match.
    VkPipelineLayout pipelineLayout;

	// The descriptor set layout describes the shader binding layout
    // (without actually referencing descriptor)
	// Like the pipeline layout it's pretty much a blueprint and can be used with
    // different descriptor sets as long as their layout matches.
    VkDescriptorSetLayout descriptorSetLayout;

	// Pipelines (often called "pipeline state objects") are used to bake all
    // states that affect a pipeline
	// Vulkan requires to layout the compute (and graphics) pipeline states upfront
	// So for each combination of non-dynamic pipeline states we need a new pipeline.
    VkPipeline pipeline;

  public:
    Kernel(Device &device, Program &program, const char *kernelName, std::vector<ResourceType> resourceTypes);
    void bindTo(VkCommandBuffer commandBuffer);
    void destroy();
};


class Arguments : protected Kernel {
  private:
    // Descriptors represent resources in shaders. They allow us
    // to use things like uniform buffers, storage buffers, etc.
    VkDescriptorPool descriptorPool;

	// The descriptor set stores the resources bound to the binding
    // points in a shader. It connects the binding points of the
    // different shaders with the buffers used for those bindings
    // A single descriptor represents a single resource, and
    // several descriptors are organized into descriptor sets,
    // which are basically just collections of descriptors.
    VkDescriptorSet descriptorSet;

  public:
    Arguments(Kernel &kernel, std::vector<Buffer> resources);
    void bindTo(VkCommandBuffer commandBuffer);
    void destroy();
};

} // end namespace vkrtl
