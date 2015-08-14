#include "SpimPlane.h"
#include "stb_image.h"

#include <GL/glew.h>
#include <GL/freeglut.h>

SpimPlane::SpimPlane(const std::string& filename)
{

}

SpimPlane::~SpimPlane()
{
	if (glIsTexture(texture))
		glDeleteTextures(1, &texture);
}
