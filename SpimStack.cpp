#include "SpimStack.h"
#include "AABB.h"
#include "Shader.h"

#include <iostream>
#include <cstring>
#include <algorithm>

#include <FreeImage.h>
#include <GL/glew.h>

using namespace std;
using namespace glm;

// voxel dimensions in microns
static const vec3 DIMENSIONS(0.625, 0.625, 3);

SpimStack::SpimStack(const string& filename) : width(0), height(0), depth(0), volume(nullptr), volumeTextureId(0), volumeList(0)
{
	FIMULTIBITMAP* fmb = FreeImage_OpenMultiBitmap(FIF_TIFF, filename.c_str(), FALSE, TRUE);


	// get the dimensions
	depth = FreeImage_GetPageCount(fmb);
	for (unsigned int z = 0; z < depth; ++z)
	{
		FIBITMAP* bm = FreeImage_LockPage(fmb, z);

		int w = FreeImage_GetWidth(bm);
		int h = FreeImage_GetHeight(bm);
		
		if (width == 0)
		{
			width = w;
			height = h;
			volume = new unsigned short[width*height*depth];
		}
		else
		{
			assert(width == w);
			assert(height = h);
		}


		unsigned short* bits = reinterpret_cast<unsigned short*>(FreeImage_GetBits(bm));
		
		size_t offset = width*height*z;
		memcpy(&volume[offset], bits, sizeof(unsigned short)*width*height);

		FreeImage_UnlockPage(fmb, bm, FALSE);
	}

	cout << "[Stack] Loaded image stack: " << width << "x" << height << "x" << depth << endl;


	FreeImage_CloseMultiBitmap(fmb);


	cout << "[Stack] Creating 3D texture ... ";
	glGenTextures(1, &volumeTextureId);
	glBindTexture(GL_TEXTURE_3D, volumeTextureId);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP);

	glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F, width, height, depth, 0, GL_RED, GL_UNSIGNED_SHORT, volume);

	cout << "done.\n";
}

SpimStack::~SpimStack()
{
	delete volume;

	glDeleteTextures(1, &volumeTextureId);
	if (glIsList(volumeList))
		glDeleteLists(volumeList, 1);
}


void SpimStack::calculatePoints(unsigned short threshold)
{
	assert(volume);
		
	// find the largest value for scaling
	unsigned short maxVal = 0;
	for (unsigned int i = 0; i < width*height*depth; ++i)
		maxVal = std::max(maxVal, volume[i]);

	points.clear();


	for (unsigned int z = 0; z < depth; ++z)
	{
		for (unsigned int x = 0; x < width; ++x)
		{
			for (unsigned int y = 0; y < height; ++y)
			{
				const unsigned short val = volume[x + y*width + z*width*height];
				if (val >= threshold)
				{
					vec3 coord(x, y, z);
					//float intensity = (float)val / maxVal;
					float intensity = (float)val;
					points.push_back(vec4(coord * DIMENSIONS, intensity));
				}

			}

		}
	}

	float relSize = (float)points.size() / (width*height*depth);
	std::cout << "[Stack] Reconstructed " << points.size() << "(" << relSize << ") points.\n";
}

AABB&& SpimStack::getBBox() const
{
	AABB bbox;
	vec3 vol = DIMENSIONS * vec3(width, height, depth);
	bbox.min = -vol * 0.5f;
	bbox.max = vol * 0.5f ;
	
	return std::move(bbox);
}

void SpimStack::drawVolume(Shader* s) const
{
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_3D, volumeTextureId);
	
	s->setUniform("volumeTexture", 0);

	if (glIsList(volumeList))
		glCallList(volumeList);
	else
	{
		volumeList = glGenLists(1);
		glNewList(volumeList, GL_COMPILE_AND_EXECUTE);
			
		glPushAttrib(GL_ENABLE_BIT);
		glDisable(GL_CULL_FACE);
		glCullFace(GL_BACK);

		glBegin(GL_QUADS);

		for (unsigned int z = 0; z < depth; ++z)
		{
			float zf = (float)z * DIMENSIONS.z;
			float zn = (float)z / depth;

			const float w = width * DIMENSIONS.x;
			const float h = height * DIMENSIONS.y;

			glTexCoord3f(0.f, 0.f, zn);
			glVertex3f(0.f, 0.f, zf);

			glTexCoord3f(1.f, 0.f, zn);
			glVertex3f(w, 0.f, zf);

			glTexCoord3f(1.f, 1.f, zn);
			glVertex3f(w, h, zf);

			glTexCoord3f(0.f, 1.f, zn);
			glVertex3f(0.f, h, zf);

		}

		glEnd();

		glPopAttrib();

		glBindTexture(GL_TEXTURE_3D, 0);
		glEndList();
	}


}
