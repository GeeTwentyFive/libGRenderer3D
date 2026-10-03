#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <stddef.h>
#include <stdint.h>

class GRenderer3D { public: ~GRenderer3D(); struct MeshInstance; private: struct _impl; std::unique_ptr<_impl> _; std::unordered_map<uint64_t, std::vector<std::unique_ptr<GRenderer3D::MeshInstance>>> mesh_instances; public:
        explicit GRenderer3D();

        float camera_pos[3] = {0, 0, 0};
        uint16_t camera_yaw = 0;  // 0..65535 = 0..2PI
        uint16_t camera_pitch = (UINT16_MAX/2);  // 16384..49152 = PI/2..PI+PI/2  (max. recommended sens. multiplier: 24 (to avoid going beyond 16384 buffer on either side in a single count (for correct clamping)))
        float camera_fov = 90.0; float camera_near = 0.01f; float camera_far = 1000.0f;

        struct Vertex { float pos[3]; float normal[3]; float texcoord[2]; };
        uint64_t CreateMesh( // returns mesh ID
                const Vertex* vertices, const size_t vertices_len,
                const uint32_t* indices, const size_t indices_len,
                const uint32_t* texture_RGBA, const uint16_t texture_width, const uint16_t texture_height
        );
        struct MeshInstance { void* user_data = nullptr; uint64_t mesh_id; GRenderer3D* _renderer_instance = nullptr;
                float position[3] = {0, 0, 0};
                float rotation[4] = {0, 0, 0, 1};  // Quaternion
                float scale[3] = {1, 1, 1};
                uint32_t color = 0xFFFFFFFF;  // Multiplied with sampled texture color
                void Remove() { for (auto it = this->_renderer_instance->mesh_instances[mesh_id].begin(); it != this->_renderer_instance->mesh_instances[mesh_id].end(); it++) { if (it->get() == this) {this->_renderer_instance->mesh_instances[mesh_id].erase(it); return;} }; }
        };
        MeshInstance* AddMesh(const uint64_t mesh_id) noexcept { auto mesh_instance = std::make_unique<MeshInstance>(); mesh_instance->_renderer_instance = this; mesh_instance->mesh_id = mesh_id; MeshInstance* ptr = mesh_instance.get(); mesh_instances[mesh_id].push_back(std::move(mesh_instance)); return ptr; }

        int DrawFrame(int framebuffer_width, int framebuffer_height) noexcept;  // returns non-0 on error
};
