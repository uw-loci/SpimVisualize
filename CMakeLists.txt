cmake_minimum_required (VERSION 2.6)
project (SpimVisualize)


SET (CMAKE_C_COMPILER             "/usr/bin/clang")
SET (CMAKE_C_FLAGS                "-Wall -std=c99")
SET (CMAKE_C_FLAGS_DEBUG          "-g")
SET (CMAKE_C_FLAGS_MINSIZEREL     "-Os -DNDEBUG")
SET (CMAKE_C_FLAGS_RELEASE        "-O4 -DNDEBUG")
SET (CMAKE_C_FLAGS_RELWITHDEBINFO "-O2 -g")

SET (CMAKE_CXX_COMPILER             "/usr/bin/clang++")
SET (CMAKE_CXX_FLAGS                "-Wall -std=c++11")
SET (CMAKE_CXX_FLAGS_DEBUG          "-g")
SET (CMAKE_CXX_FLAGS_MINSIZEREL     "-Os -DNDEBUG")
SET (CMAKE_CXX_FLAGS_RELEASE        "-O4 -DNDEBUG")
SET (CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g")

SET (CMAKE_AR      "/usr/bin/llvm-ar")
SET (CMAKE_LINKER  "/usr/bin/llvm-ld")
SET (CMAKE_NM      "/usr/bin/llvm-nm")
SET (CMAKE_OBJDUMP "/usr/bin/llvm-objdump")
SET (CMAKE_RANLIB  "/usr/bin/llvm-ranlib")

# this seems to only work on linux ...
find_package (PkgConfig REQUIRED)
find_package (OpenGL REQUIRED)
find_package (GLM REQUIRED)
find_package (GLEW REQUIRED STATIC)
find_package (FreeImage REQUIRED)

add_executable(SpimVisualize main.cpp Framebuffer.cpp OrbitCamera.cpp Shader.cpp SpimPlane.cpp SpimStack.cpp stb_image.c stb_image_write.c GeometryImage.cpp AABB.cpp GeometryImage.cpp)

LINK_DIRECTORIES("/usr/lib")
TARGET_LINK_LIBRARIES(SpimVisualize GL GLEW GLU glut freeimage)
