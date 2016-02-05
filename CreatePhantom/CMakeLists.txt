cmake_minimum_required (VERSION 3.0)
project (CreatePhantom)

# disable boost auto-linking
add_definitions( -DBOOST_ALL_NO_LIB )

set(Boost_USE_STATIC_LIBS OFF) 
set(Boost_USE_MULTITHREADED ON)  
set(Boost_USE_STATIC_RUNTIME OFF) 

SET(CMAKE_INCLUDE_PATH ${CMAKE_INCLUDE_PATH} "C:/dev/boost_1_59_0")
SET(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} "C:/dev/boost_1_59_0/lib64-msvc-12.0")

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)
if(COMPILER_SUPPORTS_CXX11)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
elseif(COMPILER_SUPPORTS_CXX0X)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
else()
        message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
endif()


find_package (OpenGL REQUIRED)
find_package (GLU REQUIRED)
find_package (GLM REQUIRED)
find_package (GLEW REQUIRED)
find_package (GLUT REQUIRED)
find_package (Boost 1.57.0 REQUIRED COMPONENTS)
find_package (FreeImage REQUIRED)

include_directories("${PROJECT_BINARY_DIR}")
include_directories("..")

#add_executable(CreatePhantom main.cpp ../AABB.h ../AABB.cpp ../Framebuffer.h ../Framebuffer.cpp ../GeometryImage.h GeometryImage.cpp glmIO.h Layout.h Layout.cpp nanoflann.hpp OrbitCamera.h OrbitCamera.cpp InteractionVolume.h InteractionVolume.cpp Ray.h Ray.cpp Shader.h Shader.cpp SimplePointcloud.h SimplePointcloud.cpp SpimPlane.h SpimPlane.cpp SpimRegistrationApp.h SpimRegistrationApp.cpp SpimStack.h SpimStack.cpp StackTransformationSolver.h StackTransformationSolver.cpp stb_image.h stb_image.c stb_image_write.c TinyStats.h)

add_executable(CreatePhantom main.cpp ../AABB.h ../AABB.cpp ../SpimStack.h ../SpimStack.cpp ../InteractionVolume.h ../InteractionVolume.cpp ../Shader.h ../Shader.cpp)

if (OPENGL_FOUND)
	#include_directories(${OPENGL_INCLUDE_DIR})
	target_link_libraries(CreatePhantom ${OPENGL_LIBRARIES})
endif (OPENGL_FOUND)

if (GLM_FOUND)
	include_directories(${GLM_INCLUDE_DIRS})
endif (GLM_FOUND)

if (GLUT_FOUND)
	include_directories(${GLUT_INCLUDE_DIR})
	target_link_libraries(CreatePhantom ${GLUT_LIBRARIES})
endif (GLUT_FOUND)

if (GLEW_FOUND)
	include_directories(${GLEW_INCLUDE_DIRS})
	target_link_libraries(CreatePhantom ${GLEW_LIBRARIES})
endif (GLEW_FOUND)

if (Boost_FOUND)
	include_directories(${Boost_INCLUDE_DIRS})
	link_directories(${Boost_LIBRARY_DIRS})
	target_link_libraries(CreatePhantom ${Boost_LIBRARIES})
endif (Boost_FOUND)

if (FREEIMAGE_FOUND)
	include_directories(${FREEIMAGE_INCLUDE_PATH})
	target_link_libraries(CreatePhantom ${FREEIMAGE_LIBRARIES})
endif (FREEIMAGE_FOUND)
