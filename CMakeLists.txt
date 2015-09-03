

cmake_minimum_required (VERSION 2.6)
project (SpimVisualize)
add_executable(SpimVisualize main.cpp Framebuffer.cpp OrbitCamera.cpp Shader.cpp SpimPlane.cpp stb_image.c GeometryImage.cpp)

