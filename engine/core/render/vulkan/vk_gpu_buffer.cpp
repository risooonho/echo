#include "vk_gpu_buffer.h"
#include "vk_mapping.h"
#include "vk_renderer.h"

namespace Echo
{
    static ui32 findMemoryType(VkPhysicalDevice vkPhysicalDevice, ui32 typeBits, VkFlags requirementsMask)
    {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(vkPhysicalDevice, &memProperties);

        for (ui32 i = 0; i < memProperties.memoryTypeCount; i++)
        {
            if ((typeBits & 1) == 1)
            {
                if ((memProperties.memoryTypes[i].propertyFlags & requirementsMask) == requirementsMask)
                    return i;
            }

            typeBits >>= 1;
        }

        // no memory types matched, return failure
        return 0;
    }

    VKBuffer::VKBuffer(GPUBufferType type, Dword usage, const Buffer& buff)
        : GPUBuffer(type, usage, buff)
    {
		updateData(buff);
    }

    VKBuffer::~VKBuffer()
    {
        clear();
    }

    bool VKBuffer::updateData(const Buffer& buff)
    {
        clear();

        VKRenderer* vkRenderer = ECHO_DOWN_CAST<VKRenderer*>(Renderer::instance());

        VkBufferCreateInfo createInfo;
        createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        createInfo.pNext = nullptr;
        createInfo.size = buff.getSize();
        createInfo.usage = VKMapping::MapGpuBufferType(m_type);
        createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
        createInfo.flags = 0;
        
        if (VK_SUCCESS == vkCreateBuffer(vkRenderer->getVkDevice(), &createInfo, nullptr, &m_vkBuffer))
        {
            VkMemoryRequirements memRequirements;
            vkGetBufferMemoryRequirements(vkRenderer->getVkDevice(), m_vkBuffer, &memRequirements);

            VkMemoryAllocateInfo allocInfo = {};
            allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocInfo.pNext = nullptr;
            allocInfo.memoryTypeIndex = 0;
            allocInfo.allocationSize = memRequirements.size;
            allocInfo.memoryTypeIndex = findMemoryType(vkRenderer->getVkPhysicalDevice(), memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

            if (VK_SUCCESS == vkAllocateMemory(vkRenderer->getVkDevice(), &allocInfo, nullptr, &m_vkBufferMemory))
            {
                vkBindBufferMemory(vkRenderer->getVkDevice(), m_vkBuffer, m_vkBufferMemory, 0);

                // filling the buffer
                void* data = nullptr;
                vkMapMemory(vkRenderer->getVkDevice(), m_vkBufferMemory, 0, memRequirements.size, 0, &data);
                memcpy(data, buff.getData(), buff.getSize());
                vkUnmapMemory(vkRenderer->getVkDevice(), m_vkBufferMemory);

                return true;
            }
        }

        EchoLogError("vulkan crete gpu buffer failed");
        return false;
    }

    void VKBuffer::bindBuffer()
    {

    }

    void VKBuffer::clear()
    {
        if (m_vkBuffer)
        {
            VKRenderer* vkRenderer = ECHO_DOWN_CAST<VKRenderer*>(Renderer::instance());
            vkDestroyBuffer(vkRenderer->getVkDevice(), m_vkBuffer, nullptr);
            m_vkBuffer = nullptr;
        }
    }
}
