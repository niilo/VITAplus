// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#pragma once

#include <renderer/state.h>
#include <renderer/types.h>

#include <renderer/vulkan/overlay_renderer.h>
#include <renderer/vulkan/pipeline_cache.h>
#include <renderer/vulkan/screen_renderer.h>
#include <renderer/vulkan/surface_cache.h>
#include <renderer/vulkan/types.h>

#include <chrono>

struct Config;

namespace renderer::vulkan {

enum class LinuxSurfaceType {
    Unknown,
    Wayland,
    Xlib,
    Xcb
};

struct Viewport {
    uint32_t offset_x;
    uint32_t offset_y;
    uint32_t width;
    uint32_t height;
    uint32_t texture_width;
    uint32_t texture_height;
};

// Select renderer paths from queried capabilities instead of GPU branding.
// Adreno stock and custom drivers can expose different Vulkan feature sets.
struct VulkanCapabilities {
    uint32_t instance_api_version = VK_API_VERSION_1_0;
    uint32_t device_api_version = VK_API_VERSION_1_0;
    uint32_t api_version = VK_API_VERSION_1_0;

    bool timeline_semaphore = false;
    bool dynamic_rendering = false;
    bool synchronization2 = false;
    bool maintenance4 = false;
    bool extended_dynamic_state = false;
    bool descriptor_indexing = false;
    bool pipeline_creation_cache_control = false;

    uint32_t subgroup_size = 0;

    uint32_t feature_mask() const {
        uint32_t mask = 0;
        mask |= timeline_semaphore ? 1U << 0 : 0;
        mask |= dynamic_rendering ? 1U << 1 : 0;
        mask |= synchronization2 ? 1U << 2 : 0;
        mask |= maintenance4 ? 1U << 3 : 0;
        mask |= extended_dynamic_state ? 1U << 4 : 0;
        mask |= descriptor_indexing ? 1U << 5 : 0;
        mask |= pipeline_creation_cache_control ? 1U << 6 : 0;
        return mask;
    }
};

struct VKState : public renderer::State {
    MemState *mem;

    // 0 = automatic, > 0 = order in instance.enumeratePhysicalDevices
    int gpu_idx;

    VKSurfaceCache surface_cache;
    PipelineCache pipeline_cache;
    VKTextureCache texture_cache;

    vk::Instance instance;
    vk::Device device;

    VulkanCapabilities capabilities;

    ScreenRenderer screen_renderer;
    OverlayRenderer overlay_renderer;

    // Used for memory allocation and general query later.
    vk::PhysicalDevice physical_device;
    vk::PhysicalDeviceProperties physical_device_properties;
    vk::PhysicalDeviceDriverProperties physical_device_driver_properties;
    bool has_physical_device_driver_properties = false;
    vk::PhysicalDeviceFeatures physical_device_features;
    vk::PhysicalDeviceMemoryProperties physical_device_memory;
    std::vector<vk::QueueFamilyProperties> physical_device_queue_families;
    vk::Format deep_stencil_use;

    vma::Allocator allocator;

    uint32_t general_family_index = 0;
    uint32_t transfer_family_index = 0;
    uint32_t general_queue_last = 0;
    uint32_t transfer_queue_last = 0;
    vk::Queue general_queue;
    vk::Queue transfer_queue;
    vk::Semaphore render_timeline;
    std::atomic<uint64_t> next_render_timeline_value{ 0 };

    // These might be merged into one queue, but for now they are different.
    vk::CommandPool general_command_pool;
    // Transfer pool has transient bit set.
    vk::CommandPool transfer_command_pool;
    // command pool which can be used from multiple thread
    vk::CommandPool multithread_command_pool;
    std::mutex multithread_pool_mutex;

    // objects for which one copy is needed for every frame being rendered at the same time
    std::array<FrameObject, MAX_FRAMES_RENDERING> frames;
    // start at 1 because last_frame_waited is set to 0
    int current_frame_idx = 1;

    // vector of descriptor pools used for the frame descriptor, they are not really used anywhere
    // but it's better to keep a reference to them somewhere
    std::deque<vk::DescriptorPool> frame_descriptor_pools;

    // only used when memory mapping is enabled
    std::map<Address, MappedMemory, std::greater<Address>> mapped_memories;
    // used with double buffer memory trapping
    BufferTrapping buffer_trapping;
    // modify the behavior of trapping on vertex buffers if there are shader stores
    bool has_shader_store = false;

    // queue where we put requests that need to wait for the GPU
    Queue<WaitThreadRequest> request_queue;

    vkutil::Image default_image;
    vkutil::Buffer default_buffer;

    bool support_fsr = false;
    // support for the VK_KHR_uniform_buffer_standard_layout extension, needed for memory mapping and texture viewport
    bool support_standard_layout = false;
    bool support_rasterized_order_access = false;
    LinuxSurfaceType linux_surface_type = LinuxSurfaceType::Unknown;

#ifdef __ANDROID__
    bool support_android_buffer_import = false;
    bool support_unix_fd_import = false;
#endif

    VKState(int gpu_idx);

    bool init() override;
    bool create(std::unique_ptr<renderer::State> &state, const Config &config);
    void late_init(const Config &cfg, const std::string_view game_id, MemState &mem) override;
    void cleanup() override;

    TextureCache *get_texture_cache() override {
        return &texture_cache;
    }

    void render_frame(DisplayState &display, const GxmState &gxm, MemState &mem) override;
    void swap_window() override;
    std::vector<uint32_t> dump_frame(DisplayState &display, uint32_t &width, uint32_t &height) override;

    uint32_t get_features_mask() override;
    int get_supported_filters() override;
    void set_screen_filter(const std::string_view &filter) override;
    int get_max_anisotropic_filtering() override;
    void set_anisotropic_filtering(int anisotropic_filtering) override;
    int get_max_2d_texture_width() override;
    void set_async_compilation(bool enable) override;

    bool map_memory(MemState &mem, Ptr<void> address, uint32_t size) override;
    void unmap_memory(MemState &mem, Ptr<void> address) override;
    // return the matching buffer and offset for the memory location
    std::tuple<vk::Buffer, uint32_t> get_matching_mapping(const Ptr<void> address);
    // return the GPU buffer device address matching this one
    uint64_t get_matching_device_address(const Address address);
#ifdef __ANDROID__
    bool support_custom_drivers() override;
    void set_turbo_mode(bool set) override;
#endif
    std::string_view get_gpu_name() override;
    uint32_t get_gpu_version() override;

    void precompile_shader(const ShadersHash &hash) override;
    void preclose_action() override;

    inline FrameObject &frame() {
        return frames[current_frame_idx];
    }
};
} // namespace renderer::vulkan
