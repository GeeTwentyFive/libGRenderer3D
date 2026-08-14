#include <GRenderer3D.hpp>

#include <glad/glad.h>
#include <linalg/linalg.h>
using namespace linalg::aliases;

#include <stdexcept>
#include <string>
#include <vector>
#include <unordered_map>
#include <string.h>
#include <math.h>
#ifndef M_PI  // (Windows fix)
#define M_PI 3.14159265358979323846
#endif


namespace {
static const size_t SSBO_VERTEX_INSTANCE_INPUT_CAPACITY = 256 * 1024 * 1024;
static const size_t SSBO_FRAGMENT_INSTANCE_INPUT_CAPACITY = 64 * 1024 * 1024;
}


#define ERROR(msg) throw std::runtime_error(std::string("[ERROR] ") + __FILE__ + "@" + std::to_string(__LINE__) + " (" + __func__ + "): " + (msg))


namespace {
const GLchar* VERTEX_SHADER = {"#version 460 core\n"
        "layout (location = 0) in vec3 pos;"
        "layout (location = 1) in vec3 normal;"
        "layout (location = 2) in vec2 uv;"
        "uniform mat4 viewProjection;"
        "layout(binding = 0, std430) readonly buffer ssboVertexInstanceInput {"
                "mat4 modelMatrices[];"
        "};"

        "out vec4 frag_worldPos;"
        "out vec3 frag_normal;"
        "out vec2 frag_uv;"
        "flat out int frag_InstanceID;"

        "void main() {" "frag_worldPos = modelMatrices[gl_InstanceID] * vec4(pos, 1.0f);" "frag_normal = normalize(mat3(transpose(inverse(modelMatrices[gl_InstanceID]))) * normal);" "frag_uv = uv;" "frag_InstanceID = gl_InstanceID;"
                "gl_Position = viewProjection * modelMatrices[gl_InstanceID] * vec4(pos, 1.0f);"
        "}"
""};
const GLchar* FRAGMENT_SHADER = {"#version 460 core\n"
        "in vec4 frag_worldPos;"
        "in vec3 frag_normal;"
        "in vec2 frag_uv;"
        "flat in int frag_InstanceID;"
        "layout(binding = 0) uniform sampler2D texture0;"
        "layout(binding = 1, std430) readonly buffer ssboFragmentInstanceInput {"
                "vec4 instanceColors[];"
        "};"
        "uniform vec3 cameraPos;"

        "out vec4 FragColor;"

        "void main() {"
                "FragColor = (texture(texture0, frag_uv) * instanceColors[frag_InstanceID]) * max(dot(frag_normal, normalize(cameraPos - frag_worldPos.xyz)), 0.1);"
        "}"
""};
}

struct Mesh {
        GLuint vao; GLsizei indices_count;
        GLuint texture_id;
};

struct GRenderer3D::_impl { uint64_t _last_uid = 0; uint64_t NewUID() { return _last_uid++; }
        GLuint shader_id;
        GLint shader_cameraPos_location; GLint shader_viewProjection_location;
        std::unordered_map<uint64_t, Mesh> meshes;

        GLuint ssbo_vertex_instance_input_id; std::vector<float4x4> instance_model_matrices;
        GLuint ssbo_fragment_instance_input_id; std::vector<float4> instance_colors;
};

GRenderer3D::GRenderer3D() { this->_ = std::make_unique<GRenderer3D::_impl>(); GLint status;
        if (!gladLoadGL()) ERROR("Failed to load OpenGL functions");

        this->_->shader_id = glCreateProgram(); if (!this->_->shader_id) ERROR("Failed to create shader program");
        GLuint vertex_shader_id = glCreateShader(GL_VERTEX_SHADER); if (!vertex_shader_id) ERROR("Failed to create vertex shader object");
        glShaderSource(vertex_shader_id, 1, &VERTEX_SHADER, NULL); glCompileShader(vertex_shader_id); glGetShaderiv(vertex_shader_id, GL_COMPILE_STATUS, &status); if (status == GL_FALSE) ERROR("Failed to compile vertex shader");
        glAttachShader(this->_->shader_id, vertex_shader_id);
        GLuint fragment_shader_id = glCreateShader(GL_FRAGMENT_SHADER); if (!fragment_shader_id) ERROR("Failed to create fragment shader object");
        glShaderSource(fragment_shader_id, 1, &FRAGMENT_SHADER, NULL); glCompileShader(fragment_shader_id); glGetShaderiv(fragment_shader_id, GL_COMPILE_STATUS, &status); if (status == GL_FALSE) ERROR("Failed to compile fragment shader");
        glAttachShader(this->_->shader_id, fragment_shader_id);
        glLinkProgram(this->_->shader_id); glGetProgramiv(this->_->shader_id, GL_LINK_STATUS, &status); if (status == GL_FALSE) ERROR("Failed to link shader program"); glDeleteShader(vertex_shader_id); glDeleteShader(fragment_shader_id);

        this->_->shader_cameraPos_location = glGetUniformLocation(this->_->shader_id, "cameraPos");
        this->_->shader_viewProjection_location = glGetUniformLocation(this->_->shader_id, "viewProjection");

        glCreateBuffers(1, &this->_->ssbo_vertex_instance_input_id);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, this->_->ssbo_vertex_instance_input_id);
        glNamedBufferStorage(this->_->ssbo_vertex_instance_input_id, SSBO_VERTEX_INSTANCE_INPUT_CAPACITY, NULL, GL_DYNAMIC_STORAGE_BIT);

        glCreateBuffers(1, &this->_->ssbo_fragment_instance_input_id);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, this->_->ssbo_fragment_instance_input_id);
        glNamedBufferStorage(this->_->ssbo_fragment_instance_input_id, SSBO_FRAGMENT_INSTANCE_INPUT_CAPACITY, NULL, GL_DYNAMIC_STORAGE_BIT);

        //glEnable(GL_DEBUG_OUTPUT); glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS); glDebugMessageCallback([](GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam){ERROR(message);}, nullptr);
}

uint64_t GRenderer3D::CreateMesh(
        const Vertex* vertices, const size_t vertices_count,
        const uint32_t* indices, const size_t indices_count,
        const uint32_t* texture_RGBA, const uint16_t texture_width, const uint16_t texture_height
) {
        uint64_t mesh_id = this->_->NewUID();
        this->_->meshes[mesh_id] = Mesh{};


        glGenVertexArrays(1, &this->_->meshes[mesh_id].vao);
        glBindVertexArray(this->_->meshes[mesh_id].vao);

        unsigned int _vbo; glGenBuffers(1, &_vbo); glBindBuffer(GL_ARRAY_BUFFER, _vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices_count*sizeof(Vertex), vertices, GL_STATIC_DRAW);

        unsigned int _ebo; glGenBuffers(1, &_ebo); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices_count*sizeof(uint32_t), indices, GL_STATIC_DRAW);
        this->_->meshes[mesh_id].indices_count = indices_count;

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texcoord));


        glCreateTextures(GL_TEXTURE_2D, 1, &this->_->meshes[mesh_id].texture_id);
        glTextureParameteri(this->_->meshes[mesh_id].texture_id, GL_TEXTURE_MAG_FILTER, GL_NEAREST); glTextureParameteri(this->_->meshes[mesh_id].texture_id, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTextureStorage2D(this->_->meshes[mesh_id].texture_id, 1, GL_RGBA8, texture_width, texture_height);
        glTextureSubImage2D(this->_->meshes[mesh_id].texture_id, 0, 0, 0, texture_width, texture_height, GL_RGBA, GL_UNSIGNED_BYTE, texture_RGBA);


        return mesh_id;
}

void GRenderer3D::MeshInstance::RotateXEuler(float degrees) noexcept { float4 new_rot = linalg::qmul(
                float4{this->rotation[0], this->rotation[1], this->rotation[2], this->rotation[3]},
                linalg::rotation_quat(float3{1, 0, 0}, (float)(degrees * 3.141592653589793238462643383279502884L/180.0))
        ); this->rotation[0] = new_rot.x; this->rotation[1] = new_rot.y; this->rotation[2] = new_rot.z; this->rotation[3] = new_rot.w;
}
void GRenderer3D::MeshInstance::RotateYEuler(float degrees) noexcept { float4 new_rot = linalg::qmul(
                float4{this->rotation[0], this->rotation[1], this->rotation[2], this->rotation[3]},
                linalg::rotation_quat(float3{0, 1, 0}, (float)(degrees * 3.141592653589793238462643383279502884L/180.0L))
        ); this->rotation[0] = new_rot.x; this->rotation[1] = new_rot.y; this->rotation[2] = new_rot.z; this->rotation[3] = new_rot.w;
}
void GRenderer3D::MeshInstance::RotateZEuler(float degrees) noexcept { float4 new_rot = linalg::qmul(
                float4{this->rotation[0], this->rotation[1], this->rotation[2], this->rotation[3]},
                linalg::rotation_quat(float3{0, 0, 1}, (float)(degrees * 3.141592653589793238462643383279502884L/180.0L))
        ); this->rotation[0] = new_rot.x; this->rotation[1] = new_rot.y; this->rotation[2] = new_rot.z; this->rotation[3] = new_rot.w;
}

int GRenderer3D::DrawFrame(int framebuffer_width, int framebuffer_height) noexcept { glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE); glEnable(GL_DEPTH_TEST);
        glUseProgram(this->_->shader_id); glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, this->_->ssbo_vertex_instance_input_id); glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, this->_->ssbo_fragment_instance_input_id);

        // Set view and projection matrices (Camera)
        float3 _camera_pos = {camera_pos[0], camera_pos[1], camera_pos[2]};
        glUniform3fv(
                this->_->shader_cameraPos_location, 1,
                (const GLfloat*)&_camera_pos
        );
        if (camera_pitch < 16385) camera_pitch = 16385; if (camera_pitch > 49151) camera_pitch = 49151;  // 16384..49152 = PI/2..PI+PI/2
        float4x4 view = linalg::lookat_matrix(
                _camera_pos,
                _camera_pos + linalg::qrot(
                        linalg::qmul(
                                linalg::rotation_quat(float3{0, 1, 0}, -(float)(camera_yaw*(2*M_PI/65536))),
                                linalg::rotation_quat(float3{1, 0, 0}, (float)(camera_pitch*(2*M_PI/65536)))
                        ),
                        float3{0, 0, -1}
                ),
                float3{0, 1, 0}
        );
        float4x4 proj = linalg::perspective_matrix(camera_fov, ((float)framebuffer_width)/((float)framebuffer_height), camera_near, camera_far, linalg::fwd_axis::neg_z, linalg::z_range::zero_to_one);
        float4x4 viewProjection = linalg::mul(proj, view);  // NOTE: Flip multiplication order in not-OpenGL
        glUniformMatrix4fv(
                this->_->shader_viewProjection_location, 1, GL_FALSE,
                (const GLfloat*)&viewProjection
        );

        // Draw meshes
        for (const auto& [mesh_id, _mesh_instances] : mesh_instances) {
                glBindVertexArray(this->_->meshes[mesh_id].vao);
                glBindTextureUnit(0, this->_->meshes[mesh_id].texture_id);

                this->_->instance_model_matrices.clear(); this->_->instance_model_matrices.reserve(_mesh_instances.size()); if (this->_->instance_model_matrices.capacity() > SSBO_VERTEX_INSTANCE_INPUT_CAPACITY) return __LINE__;
                this->_->instance_colors.clear(); this->_->instance_colors.reserve(_mesh_instances.size()); if (this->_->instance_colors.capacity() > SSBO_FRAGMENT_INSTANCE_INPUT_CAPACITY) return __LINE__;
                for (const auto& mesh_instance : _mesh_instances) {
                        this->_->instance_model_matrices.push_back(linalg::mul(
                                linalg::translation_matrix(float3{
                                        mesh_instance->position[0],
                                        mesh_instance->position[1],
                                        mesh_instance->position[2]
                                }),
                                linalg::mul(
                                        linalg::rotation_matrix(float4{
                                                mesh_instance->rotation[0],
                                                mesh_instance->rotation[1],
                                                mesh_instance->rotation[2],
                                                mesh_instance->rotation[3]
                                        }),
                                        linalg::scaling_matrix(float3{
                                                mesh_instance->scale[0],
                                                mesh_instance->scale[1],
                                                mesh_instance->scale[2]
                                        })
                                )
                        ));

                        this->_->instance_colors.push_back({
                                ((mesh_instance->color_RGBA >> 24) & 0xFF) / 255.0f,
                                ((mesh_instance->color_RGBA >> 16) & 0xFF) / 255.0f,
                                ((mesh_instance->color_RGBA >> 8) & 0xFF) / 255.0f,
                                ((mesh_instance->color_RGBA >> 0) & 0xFF) / 255.0f
                        });
                }
                glNamedBufferSubData(this->_->ssbo_vertex_instance_input_id, 0, this->_->instance_model_matrices.size()*sizeof(float4x4), this->_->instance_model_matrices.data());
                glNamedBufferSubData(this->_->ssbo_fragment_instance_input_id, 0, this->_->instance_colors.size()*sizeof(float4), this->_->instance_colors.data());

                glDrawElementsInstanced(GL_TRIANGLES, this->_->meshes[mesh_id].indices_count, GL_UNSIGNED_INT, 0, _mesh_instances.size());
        }


        return 0;
}

GRenderer3D::~GRenderer3D() { glDeleteProgram(this->_->shader_id); }