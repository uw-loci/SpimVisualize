#include "SpimPlane.h"
#include "Shader.h"

#include <FreeImage.h>
#include <GL/glew.h>
#include <GL/freeglut.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/io.hpp>

#include <iostream>
#include <fstream>
#include <cassert>
#include <cstdio>
#include <string>

SpimPlane::SpimPlane(const std::string& imageFile, const std::string& registrationFile) : texture(0), transform(1.f)
{

	// read the image
	FREE_IMAGE_FORMAT fif = FreeImage_GetFileType(imageFile.c_str(), 0);
	assert(fif == FIF_TIFF);

	FIBITMAP* bitmap = FreeImage_Load(fif, imageFile.c_str());
	assert(bitmap);

	unsigned int w = FreeImage_GetWidth(bitmap);
	unsigned int h = FreeImage_GetHeight(bitmap);
	unsigned bpp = FreeImage_GetBPP(bitmap);
	std::cout << "[SpimPlane] Loaded image: " << w << "x" << h << "x" << bpp << std::endl;
	
	// make sure it is a 16bit grayscale image (see documentation)
	FREE_IMAGE_COLOR_TYPE fct = FreeImage_GetColorType(bitmap);
	assert(fct == FIC_MINISBLACK);
	assert(bpp == 16);
	
	// make sure it is actually loaded
	assert(FreeImage_HasPixels(bitmap));




	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	
	BYTE* data = FreeImage_GetBits(bitmap);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, w, h, 0, GL_LUMINANCE, GL_UNSIGNED_SHORT, data);

	FreeImage_Unload(bitmap);


	// read the registration matrix
	std::ifstream file(registrationFile);
	assert(file.is_open());

	for (int i = 0; i < 16; ++i)
	{
		std::string buffer;
		std::getline(file, buffer);

		int result = sscanf(buffer.c_str(), "m%*2s: %f", &glm::value_ptr(transform)[i]);
	}
	
	transform = glm::transpose(transform);
	std::cout << "[SpimPlane] Read transform: " << transform << std::endl;

}

SpimPlane::~SpimPlane()
{
	if (glIsTexture(texture))
		glDeleteTextures(1, &texture);
}

void SpimPlane::draw(Shader* s) const
{
	s->setMatrix4("transform", transform);
	
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);
	s->setUniform("image", 0);
	
	// draw two quads
	int vertices[] = { -1, 1, -1, -1, 1, -1, 1, 1 };
	unsigned char indices[] = { 0, 1, 2, 3, 3, 2, 1, 0 };

	glEnableClientState(GL_VERTEX_ARRAY);

	glVertexPointer(2, GL_INT, 0, vertices);
	glDrawElements(GL_QUADS, 8, GL_UNSIGNED_BYTE, indices);


	glDisableClientState(GL_VERTEX_ARRAY);



}
