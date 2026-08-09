#include <GWindower.hpp>
#include <glad/glad.h>
#include "../include/GRenderer3D.hpp"
#include <GOBJ/GOBJ.hpp>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include <stb/stb_image.h>
#include <linalg/linalg.h>
using namespace linalg::aliases;
#include <GFramePacer/GFramePacer.hpp>

#include <stdexcept>
#include <string>
#include <math.h>
#include <iostream>


const double TARGET_FRAMETIME_MS = 2.0;  // Should be a power-of-2 (like 1.0, 2.0, 4.0, 8.0) (to align as fraction/multiple of USB input report rate)

const int CAMERA_SENSITIVITY = 16;
const int CAMERA_MOVE_SPEED = 8;


#define ERROR(msg) throw std::runtime_error(std::string("[ERROR] ") + __FILE__ + "@" + std::to_string(__LINE__) + " (" + __func__ + "): " + (msg))


int main() { try {
        GWindower gw{0, 0, 4, 6}; if (!gladLoadGL()) ERROR("Failed to load OpenGL");
        glViewport(0, 0, GWindower::GetScreenWidth(), GWindower::GetScreenHeight()); glClearColor(0.0, 0.0, 0.0, 1.0);

        GRenderer3D gr3d{(uint32_t)GWindower::GetScreenWidth(), (uint32_t)GWindower::GetScreenHeight()};

        GOBJ cube_mesh_data("Cube.obj");
        int texture_width, texture_height, _texture_comp;
        unsigned char* texture = stbi_load("Cube.png", &texture_width, &texture_height, &_texture_comp, 4);
        uint64_t cube_mesh_id = gr3d.CreateMesh(
                (GRenderer3D::Vertex*)cube_mesh_data.vertices.data(), cube_mesh_data.vertices.size(),
                (uint32_t*)cube_mesh_data.indices.data(), cube_mesh_data.indices.size(),
                (uint32_t*)texture, texture_width, texture_height
        );
        stbi_image_free(texture);

        GRenderer3D::MeshInstance* cube_mesh_instance_1 = gr3d.AddMesh(cube_mesh_id);
        cube_mesh_instance_1->scale[0] = 32.0f; cube_mesh_instance_1->scale[2] = 32.0f;
        cube_mesh_instance_1->position[1] = -2.0f;
        GRenderer3D::MeshInstance* cube_mesh_instance_2 = gr3d.AddMesh(cube_mesh_id);
        cube_mesh_instance_2->RotateZEuler(10.0f);

        GFramePacer gfp; gfp.target_frametime_ms = TARGET_FRAMETIME_MS;
        while (gw.Update()) { if(gw.key_states[GWindower::KEY_ESCAPE]) break;
                double delta_time = gfp.Wait();

                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                gr3d.camera_yaw += gw.mouse_x * CAMERA_SENSITIVITY;
                gr3d.camera_pitch += gw.mouse_y * CAMERA_SENSITIVITY;
                gr3d.camera_pos[1] += (-gw.key_states[GWindower::KEY_LEFT_CONTROL] + gw.key_states[GWindower::KEY_SPACE]) * CAMERA_MOVE_SPEED * delta_time;
                float2 move_dir = float2{
                        (float)(-gw.key_states[GWindower::KEY_A] + gw.key_states[GWindower::KEY_D]),
                        (float)(-gw.key_states[GWindower::KEY_W] + gw.key_states[GWindower::KEY_S])
                };
                if (linalg::length2(move_dir) != 0) move_dir = linalg::normalize(move_dir);
                move_dir = linalg::rot((float)(gr3d.camera_yaw*(2*M_PI/65536)), move_dir);
                gr3d.camera_pos[0] += -move_dir.x * CAMERA_MOVE_SPEED * delta_time;
                gr3d.camera_pos[2] += -move_dir.y * CAMERA_MOVE_SPEED * delta_time;

                gr3d.DrawFrame();
        }

        return 0;
} catch (const std::exception& e) { std::cout << e.what() << std::endl; return 1; } }