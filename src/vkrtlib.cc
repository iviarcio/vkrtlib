// NAME
//   vkrtlib.cc
// VERSION
//    0.1
// SYNOPSIS
//    Vulkan runtime library -- vrtlib -- is a GPGPU engine based on Vulkan.
//    It eats SPIR-V compute shaders and executes them without any graphical
//    context, much like OpenCL. The interface is very abstract and allows you
//    to get to work with your shaders as quickly as possible. With the current
//    state of compute shaders, you should be able to achieve OpenCL 1.2
//    feature parity.
// AUTHOR
//    Marcio Machado Pereira

#include "vkrtlib.h"
#include <iostream>
#include <fstream>
#include <vector>

namespace vkrtl {

uint32_t _verbose;
uint32_t _profile;

static VKAPI_ATTR VkBool32 VKAPI_CALL debugReportCallbackFn(
        VkDebugReportFlagsEXT flags,
        VkDebugReportObjectTypeEXT objectType,
        uint64_t object,
        size_t location,
        int32_t messageCode,
        const char* pLayerPrefix,
        const char* pMessage,
        void* pUserData) {
    std::cout << "[vkrtl] " << pLayerPrefix << " " << pMessage << std::endl;
    return VK_FALSE;
}

void DestroyDebugReportCallbackEXT(VkInstance instance,
        VkDebugReportCallbackEXT callback,
        const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugReportCallbackEXT) vkGetInstanceProcAddr(instance,
            "vkDestroyDebugReportCallbackEXT");
    if (func != nullptr) {
        func(instance, callback, pAllocator);
    }
}


Object::Object(enum ModeOptions mode) {

    _verbose = mode == VKRTL_verbose || mode == VKRTL_all;
    _profile = mode == VKRTL_profile || mode == VKRTL_all;

    VkApplicationInfo applicationInfo = {};
    applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.flags = 0;
    createInfo.pApplicationInfo = &applicationInfo;

    // Validation layers are optional components that hook into Vulkan
    // function calls to apply additional operations like tracing Vulkan
    // calls for logging every call and profiling.
    // By enabling validation layers, Vulkan will emit warnings if the API
    // is used incorrectly.
    // We shall enable the layer VK_LAYER_LUNARG_standard_validation,
    // which is basically a collection of several useful validation layers.
    uint32_t layerCount = 1;
    const char* validationLayers[] = { "VK_LAYER_LUNARG_standard_validation" };

    // We get all supported layers with vkEnumerateInstanceLayerProperties.
    uint32_t instanceLayerCount;
    vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr);
    std::vector<VkLayerProperties> instanceLayers(instanceLayerCount);
    vkEnumerateInstanceLayerProperties(&instanceLayerCount, instanceLayers.data());

    if (_verbose) {
        std::cout << "[vkrtl] available vulkan layers:" << std::endl;
        for (const auto& layer : instanceLayers) {
            std::cout << "[vkrtl]     name: " << layer.layerName << " desc: " 
                      << layer.description << " impl_ver: "
            << VK_VERSION_MAJOR(layer.implementationVersion) << "."
            << VK_VERSION_MINOR(layer.implementationVersion) << "."
            << VK_VERSION_PATCH(layer.implementationVersion)
            << " spec_ver: "
            << VK_VERSION_MAJOR(layer.specVersion) << "."
            << VK_VERSION_MINOR(layer.specVersion) << "."
            << VK_VERSION_PATCH(layer.specVersion)
            << std::endl;
        }
    }
    // And then we simply check if VK_LAYER_LUNARG_standard_validation
    // is among the supported layers.
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
        createInfo.ppEnabledLayerNames = validationLayers;
        createInfo.enabledLayerCount = layerCount;
        createInfo.enabledExtensionCount = 0;
        if (_verbose) {
            // We enable an extension named VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
            // in order to be able to print the warnings emitted by the validation layer.
            createInfo.enabledExtensionCount = 1;
            const char *validationExt = VK_EXT_DEBUG_REPORT_EXTENSION_NAME;
            createInfo.ppEnabledExtensionNames = &validationExt;
        }
    }

    if (VK_SUCCESS != vkCreateInstance(&createInfo, nullptr, &instance)) {
        throw VKRTL_ERROR_CREATE_INSTANCE;
    }

    if (_verbose) {
        // Register a callback function for the extension
        // VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
        // so that warnings emitted from the validation layer are printed.
        VkDebugReportCallbackCreateInfoEXT createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
        createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT |
            VK_DEBUG_REPORT_WARNING_BIT_EXT |
            VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
        createInfo.pfnCallback = &debugReportCallbackFn;

        // We have to explicitly load this function.
        auto vkCreateDebugReportCallbackEXT =
            (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance,
                    "vkCreateDebugReportCallbackEXT");
        if (vkCreateDebugReportCallbackEXT == nullptr) {
            throw VKRTL_ERROR_CREATE_REPORT_CALLBACK;
        }

        // Create and register callback.
        vkCreateDebugReportCallbackEXT(instance, &createInfo, NULL, &debugReportCallback);
    }

    // First we will list all physical devices on the system with vkEnumeratePhysicalDevices.
    uint32_t numDevices;
    if (VK_SUCCESS !=
          vkEnumeratePhysicalDevices(instance, &numDevices, nullptr) || !numDevices) {
        throw VKRTL_ERROR_DEVICES;
    }

    VkPhysicalDevice *physicalDevices = new VkPhysicalDevice[numDevices];
    if (VK_SUCCESS != vkEnumeratePhysicalDevices(instance, &numDevices, physicalDevices)) {
        throw VKRTL_ERROR_DEVICES;
    }

    for (uint32_t i = 0; i < numDevices; i++) {
        devices.push_back(Device(physicalDevices[i]));
    }

    delete[] physicalDevices;
}

Object::~Object() {
    if (_verbose) {
        DestroyDebugReportCallbackEXT(instance, debugReportCallback, nullptr);
        std::cout << "[vkrtl] clean up Vulkan Object." << std::endl;
    }
    vkDestroyInstance(instance, nullptr);
}

std::vector<Device> &Object::getDevices() {
    return devices;
}

Device &Object::getDevice() {
    return devices[0];
}

VkInstance &Object::getInstance() {
    return instance;
}


Device::Device(VkPhysicalDevice physicalDevice) : physicalDevice(physicalDevice) {

    // select a queue family with compute support
    uint32_t numQueues;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &numQueues, nullptr);

    VkQueueFamilyProperties *queueFamilyProperties = new VkQueueFamilyProperties[numQueues];
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &numQueues, queueFamilyProperties);

    for (uint32_t i = 0; i < numQueues; i++) {
        // choose only the queue in this queue family that support compute operations.
        if (queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            computeQueueFamily = i;
            break;
        }
    }

    delete[] queueFamilyProperties;
    if (computeQueueFamily == -1) {
        throw VKRTL_ERROR_COMPUTE_QUEUE;
    }

    VkDeviceQueueCreateInfo queueCreateInfo = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queueCreateInfo.queueCount = 1;
    float priorities[] = {1.0f};
    queueCreateInfo.pQueuePriorities = priorities;
    queueCreateInfo.queueFamilyIndex = computeQueueFamily;

    // create the logical device
    VkPhysicalDeviceFeatures physicalDeviceFeatures = {};
    VkDeviceCreateInfo deviceCreateInfo = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceCreateInfo.pEnabledFeatures = &physicalDeviceFeatures;
    deviceCreateInfo.queueCreateInfoCount = 1;
    if (VK_SUCCESS != vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device)) {
        throw VKRTL_ERROR_DEVICES;
    }

    vkGetDeviceQueue(device, computeQueueFamily, 0, &queue);

    // With VkPhysicalDeviceProperties() we obtain a list of physical device limitations.
    // This library launches a compute shader, and the maximum size of the workgroups and
    // total number of compute shader invocations is limited by the physical device, and
    // we should ensure that the limitations named maxComputeWorkGroupCount,
    // maxComputeWorkGroupInvocations and maxComputeWorkGroupSize are not exceeded by the application.
    // Moreover, we are using a storage buffer in the compute shader, and we should ensure that
    // it is not larger than the device can handle, by checking the limitation maxStorageBufferRange.

    vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

    // get indices of memory types we care about
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &physicalDeviceMemoryProperties);
    for (uint32_t i = 0; i < physicalDeviceMemoryProperties.memoryTypeCount; i++) {

        // VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT bit specifies that memory allocated
        // with this type can be mapped for host access using vkMapMemory.
        if (physicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT &&
            memoryTypeMappable == -1) {
            memoryTypeMappable = i;
        }
        // VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT bit specifies that memory allocated
        // with this type is the most efficient for device access.
        if (physicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT &&
            memoryTypeLocal == -1) {
            memoryTypeLocal = i;
        }
    }

    if (_verbose) showProperties();

    // create the implicit command buffer
    implicitCommandBuffer = new CommandBuffer(*this);
}

void Device::showProperties() {
    VkPhysicalDeviceFeatures physicalDeviceFeatures;
    std::vector<VkExtensionProperties> physicalDeviceExtensions;
    uint32_t extension_count;

    vkGetPhysicalDeviceFeatures(physicalDevice, &physicalDeviceFeatures);
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extension_count, nullptr);
    physicalDeviceExtensions.resize(extension_count);
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extension_count,
         physicalDeviceExtensions.data());

    // print info
    std::cout << "[vkrtl] selected device name: " << physicalDeviceProperties.deviceName
        << std::endl << "[vkrtl] selected device type: ";
    switch (physicalDeviceProperties.deviceType)
    {
    case VK_PHYSICAL_DEVICE_TYPE_OTHER:
        std::cout << "VK_PHYSICAL_DEVICE_TYPE_OTHER";
        break;
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
        std::cout << "VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU";
        break;
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
        std::cout << "VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU";
        break;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
        std::cout << "VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU";
        break;
    case VK_PHYSICAL_DEVICE_TYPE_CPU:
        std::cout << "VK_PHYSICAL_DEVICE_TYPE_CPU";
        break;
    default:
        ;
    }
    std::cout << " (" << physicalDeviceProperties.deviceType << ")" << std::endl
        << "[vkrtl] selected device driver version: "
        << VK_VERSION_MAJOR(physicalDeviceProperties.driverVersion) << "."
        << VK_VERSION_MINOR(physicalDeviceProperties.driverVersion) << "."
        << VK_VERSION_PATCH(physicalDeviceProperties.driverVersion) << std::endl
        << "[vkrtl] selected device vulkan api version: "
        << VK_VERSION_MAJOR(physicalDeviceProperties.apiVersion) << "."
        << VK_VERSION_MINOR(physicalDeviceProperties.apiVersion) << "."
        << VK_VERSION_PATCH(physicalDeviceProperties.apiVersion) << std::endl;
    std::cout << "[vkrtl] selected device available extensions:" << std::endl;
    for (const auto& extension : physicalDeviceExtensions)
    {
        std::cout << "[vkrtl]     name: " << extension.extensionName << " spec_ver: "
            << VK_VERSION_MAJOR(extension.specVersion) << "."
            << VK_VERSION_MINOR(extension.specVersion) << "."
            << VK_VERSION_PATCH(extension.specVersion) << std::endl;
    }
}

void Device::destroy() {
    implicitCommandBuffer->destroy();
    delete implicitCommandBuffer;
    vkDestroyDevice(device, nullptr);
    if (_verbose)
        std::cout << "[vkrtl] clean up Vulkan Device." << std::endl;
}

void Device::submit(VkCommandBuffer commandBuffer) {
    VkSubmitInfo submitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    VkCommandBuffer commandBuffers[1] = {commandBuffer};
    submitInfo.pCommandBuffers = commandBuffers;
    if (VK_SUCCESS != vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE)) {
        throw VKRTL_ERROR_SUBMIT_QUEUE;
    }
}

void Device::wait() {
    if (VK_SUCCESS != vkQueueWaitIdle(queue)) {
        throw VKRTL_ERROR_SUBMIT_QUEUE;
    }
}

const char *Device::getName() {
    return physicalDeviceProperties.deviceName;
}

uint32_t Device::getVendorId() {
    return physicalDeviceProperties.vendorID;
}


void CommandBuffer::sharedConstructor() {
    // Command pools are used mainly as a source of memory for the command buffers
    // When we use VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, we can reset
    // command buffers individually. Command pools also control the queues to which
    // command buffers can be submitted. This is achieved through a queue family index
    VkCommandPoolCreateInfo commandPoolCreateInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolCreateInfo.queueFamilyIndex = this->computeQueueFamily;
    if (VK_SUCCESS != vkCreateCommandPool(this->device, &commandPoolCreateInfo, nullptr, &commandPool)) {
        throw VKRTL_ERROR_COMMAND_POOL;
    }

    // Command buffers are allocated from command pools and can be submitted
    // only to queues from the specified family.
    VkCommandBufferAllocateInfo commandBufferAllocateInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    commandBufferAllocateInfo.commandBufferCount = 1;
    // Primary command buffers can be directly submitted to queues.
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandPool = commandPool;
    if (VK_SUCCESS != vkAllocateCommandBuffers(this->device, &commandBufferAllocateInfo, &commandBuffer)) {
        throw VKRTL_ERROR_COMMAND_BUFFER;
    }
}

CommandBuffer::CommandBuffer(Device &device) : Device(device) {
    sharedConstructor();
}

CommandBuffer::CommandBuffer(Device &device, Kernel &kernel, Arguments &arguments) : Device(device) {
    sharedConstructor();
    begin();
    arguments.bindTo(*this);
    kernel.bindTo(*this);
}

void CommandBuffer::destroy() {
    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    vkDestroyCommandPool(device, commandPool, nullptr);
}

CommandBuffer::operator VkCommandBuffer() {
    return commandBuffer;
}

void CommandBuffer::begin() {
    VkCommandBufferBeginInfo commandBufferBeginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    if (VK_SUCCESS != vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo)) {
        throw VKRTL_ERROR_COMMAND_BUFFER;
    }
}

void CommandBuffer::barrier() {
    vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0,
            nullptr, 0, nullptr, 0, nullptr);
}

void CommandBuffer::dispatch(int x, int y, int z) {
    vkCmdDispatch(commandBuffer, x, y, z);
}

void CommandBuffer::end() {
    if (VK_SUCCESS != vkEndCommandBuffer(commandBuffer)) {
        throw VKRTL_ERROR_COMMAND_BUFFER;
    }
}

Buffer::Buffer(Device &device, size_t byteSize, bool mappable) : Device(device) {
    // create buffer
    VkBufferCreateInfo bufferCreateInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferCreateInfo.size = byteSize;
    // specifies that the buffer can be used in a VkDescriptorBufferInfo suitable
    // for occupying a VkDescriptorSet slot either of type
    // VK_DESCRIPTOR_TYPE_STORAGE_BUFFER or VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC
    bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    // specifies that the buffer can be used as the source of a transfer command
    bufferCreateInfo.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    // specifies that the buffer can be used as the destination of a transfer command
    bufferCreateInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (VK_SUCCESS != vkCreateBuffer(this->device, &bufferCreateInfo, nullptr, &buffer)) {
        throw VKRTL_ERROR_CREATE_BUFFER;
    }

    // get memory requirements
    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(this->device, buffer, &memoryRequirements);

    // allocate memory for the buffer
    VkMemoryAllocateInfo memoryAllocateInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    memoryAllocateInfo.allocationSize = memoryRequirements.size;
    memoryAllocateInfo.memoryTypeIndex = mappable ? memoryTypeMappable : memoryTypeLocal;
    if (VK_SUCCESS != vkAllocateMemory(this->device, &memoryAllocateInfo, nullptr, &memory)) {
        throw VKRTL_ERROR_MALLOC;
    }

    // bind memory to the buffer
    if (VK_SUCCESS != vkBindBufferMemory(this->device, buffer, memory, 0)) {
        throw VKRTL_ERROR_MALLOC;
    }

    if (_verbose) {
        std::string info = "";
        if (mappable) info = "mappable ";
        std::cout << "[vrtl] Create a "<< info << "buffer of " <<
            byteSize << " bytes" << std::endl;
    }
}

void Buffer::enqueueCopy(Buffer src, Buffer dst, size_t byteSize, VkCommandBuffer commandBuffer) {
    VkBufferCopy bufferCopy = {0, 0, byteSize};
    vkCmdCopyBuffer(commandBuffer, src.buffer, dst.buffer, 1, &bufferCopy);
}

void Buffer::inload(void *hostPtr) {
    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(this->device, buffer, &memoryRequirements);

    Buffer mappable(*this, memoryRequirements.size, memoryTypeMappable);

    implicitCommandBuffer->begin();
    enqueueCopy(*this, mappable, memoryRequirements.size, *implicitCommandBuffer);
    implicitCommandBuffer->end();
    submit(*implicitCommandBuffer);
    wait();

    memcpy(hostPtr, mappable.map(), memoryRequirements.size);
    mappable.unmap();
    mappable.destroy();
}

void Buffer::offload(void *hostPtr) {
    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(this->device, buffer, &memoryRequirements);

    Buffer mappable(*this, memoryRequirements.size, memoryTypeMappable);
    memcpy(mappable.map(), hostPtr, memoryRequirements.size);
    mappable.unmap();

    implicitCommandBuffer->begin();
    enqueueCopy(mappable, *this, memoryRequirements.size, *implicitCommandBuffer);
    implicitCommandBuffer->end();
    submit(*implicitCommandBuffer);
    wait();

    mappable.destroy();
}

Buffer::operator VkBuffer() {
    return buffer;
}

void Buffer::destroy() {
    if (_verbose) {
        // get memory requirements
        VkMemoryRequirements memoryRequirements;
        vkGetBufferMemoryRequirements(this->device, buffer, &memoryRequirements);
        std::cout << "[vkrtl] destroy buffer. Size equals " << memoryRequirements.size << std::endl;
    }
    vkFreeMemory(device, memory, nullptr);
    vkDestroyBuffer(device, buffer, nullptr);
}

void Buffer::unmap() {
    vkUnmapMemory(device, memory);
}

void *Buffer::map() {
    double *pointer;
    if (VK_SUCCESS != vkMapMemory(device, memory, 0, VK_WHOLE_SIZE, 0, (void **)&pointer)) {
        throw VKRTL_ERROR_MAP;
    }
    return pointer;
}

Program::Program(Device &device, const char *fileName) : Device(device) {
    std::ifstream fin(fileName, std::ifstream::ate);
    size_t byteLength = fin.tellg();
    fin.seekg(0, std::ifstream::beg);
    char *data = new char[byteLength];
    fin.read(data, byteLength);
    fin.close();

    VkShaderModuleCreateInfo shaderModuleCreateInfo = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    shaderModuleCreateInfo.codeSize = byteLength;
    shaderModuleCreateInfo.pCode = (uint32_t *)data;
    if (VK_SUCCESS != vkCreateShaderModule(this->device, &shaderModuleCreateInfo, nullptr, &shaderModule)) {
        throw VKRTL_ERROR_SHADER;
    }
    delete[] data;
}

Program::Program(Device &device, uint32_t *data) : Device(device) {
    VkShaderModuleCreateInfo shaderModuleCreateInfo = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    shaderModuleCreateInfo.codeSize = sizeof(data);
    shaderModuleCreateInfo.pCode = data;
    if (VK_SUCCESS != vkCreateShaderModule(this->device, &shaderModuleCreateInfo, nullptr, &shaderModule)) {
        throw VKRTL_ERROR_SHADER;
    }
}

void Program::destroy() {
    vkDestroyShaderModule(device, shaderModule, nullptr);
    if (_verbose)
        std::cout << "[vkrtl] destroy the Program." << std::endl;
}


Kernel::Kernel(Device &device, Program &program, const char *kernelName, std::vector<ResourceType> resourceTypes)
    : Program(program) {

    VkDescriptorSetLayoutBinding *bindings = new VkDescriptorSetLayoutBinding[resourceTypes.size()];
    for (uint32_t i = 0; i < resourceTypes.size(); i++) {
        bindings[i].binding = i;
        bindings[i].descriptorType = (VkDescriptorType)resourceTypes[i];
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        bindings[i].pImmutableSamplers = nullptr;
    }

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    descriptorSetLayoutCreateInfo.pBindings = bindings;
    descriptorSetLayoutCreateInfo.bindingCount = resourceTypes.size();
    if (VK_SUCCESS !=
        vkCreateDescriptorSetLayout(this->device, &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayout)) {
        throw VKRTL_ERROR_SHADER;
    }

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;
    if (VK_SUCCESS != vkCreatePipelineLayout(this->device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout)) {
        throw VKRTL_ERROR_SHADER;
    }

    delete[] bindings;

    VkPipelineShaderStageCreateInfo pipelineShaderInfo = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    pipelineShaderInfo.module = shaderModule;
    pipelineShaderInfo.pName = kernelName;
    pipelineShaderInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;

    VkComputePipelineCreateInfo pipelineInfo = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    pipelineInfo.stage = pipelineShaderInfo;
    pipelineInfo.layout = pipelineLayout;
    if (VK_SUCCESS != vkCreateComputePipelines(this->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline)) {
        throw VKRTL_ERROR_PIPELINE;
    }
}

void Kernel::bindTo(VkCommandBuffer commandBuffer) {
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
}

void Kernel::destroy() {
    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyPipeline(device, pipeline, nullptr);
    if (_verbose)
        std::cout << "[vkrtl] destroy the Kernel." << std::endl;
}

Arguments::Arguments(Kernel &kernel, std::vector<Buffer> resources) : Kernel(kernel) {
    // how many of each type
    VkDescriptorPoolSize descriptorPoolSizes[] = {{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, (uint32_t)resources.size()}};

    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    descriptorPoolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    descriptorPoolCreateInfo.poolSizeCount = 1;
    descriptorPoolCreateInfo.maxSets = 1;
    descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSizes;
    if (VK_SUCCESS != vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, nullptr, &descriptorPool)) {
        throw VKRTL_ERROR_DESCRIPTOR;
    }

    // allocate the descriptor set (according to the function's layout)
    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    descriptorSetAllocateInfo.descriptorSetCount = 1;
    descriptorSetAllocateInfo.pSetLayouts = &descriptorSetLayout;
    descriptorSetAllocateInfo.descriptorPool = descriptorPool;

    if (VK_SUCCESS != vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSet)) {
        throw VKRTL_ERROR_DESCRIPTOR;
    }

    // buffers to bind
    VkDescriptorBufferInfo *descriptorBufferInfo = new VkDescriptorBufferInfo[resources.size()];
    for (uint32_t i = 0; i < resources.size(); i++) {
        descriptorBufferInfo[i].buffer = resources[i];
        descriptorBufferInfo[i].offset = 0;
        descriptorBufferInfo[i].range = VK_WHOLE_SIZE;
    }

    // bind stuff here
    VkWriteDescriptorSet writeDescriptorSet = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    writeDescriptorSet.dstSet = descriptorSet; // pipeline.getDescriptorSet();
    writeDescriptorSet.dstBinding = 0;
    writeDescriptorSet.descriptorCount = resources.size();
    writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writeDescriptorSet.pBufferInfo = descriptorBufferInfo;
    vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
    delete[] descriptorBufferInfo;
}

void Arguments::bindTo(VkCommandBuffer commandBuffer) {
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0,
                            nullptr);
}

void Arguments::destroy() {
    vkFreeDescriptorSets(device, descriptorPool, 1, &descriptorSet);
    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    if (_verbose)
        std::cout << "[vkrtl] Destroy arguments." << std::endl;
}

} // end namespace vkrtl
