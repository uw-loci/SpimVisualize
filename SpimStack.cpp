#include "SpimStack.h"
#include "AABB.h"
#include "Shader.h"

#include <iostream>
#include <cstring>
#include <algorithm>
#include <fstream>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/io.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <FreeImage.h>
#include <GL/glew.h>

using namespace std;
using namespace glm;

// voxel dimensions in microns
static const vec3 DIMENSIONS(0.625, 0.625, 3);

SpimStack::SpimStack(const string& filename) : width(0), height(0), depth(0), volume(nullptr), volumeTextureId(0), transform(1.f), enabled(true)
{
	volumeList[0] = 0;
	volumeList[1] = 0;

	FIMULTIBITMAP* fmb = FreeImage_OpenMultiBitmap(FIF_TIFF, filename.c_str(), FALSE, TRUE);
	assert(fmb);


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


	cout << "[Stack] Updating stats ... ";
	maxVal = 0;
	minVal = std::numeric_limits<unsigned short>::max();
	for (size_t i = 0; i < width*height*depth; ++i)
	{
		unsigned short val = volume[i];
		maxVal = std::max(maxVal, val);
		minVal = std::min(minVal, val);
	}
	cout << "done; range: " << minVal << "-" << maxVal << endl;


	cout << "[Stack] Creating 3D texture ... ";
	glGenTextures(1, &volumeTextureId);
	glBindTexture(GL_TEXTURE_3D, volumeTextureId);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP);

	glTexImage3D(GL_TEXTURE_3D, 0, GL_R16I, width, height, depth, 0, GL_RED_INTEGER, GL_UNSIGNED_SHORT, volume);

	cout << "done.\n";
}

SpimStack::~SpimStack()
{
	delete volume;

	glDeleteTextures(1, &volumeTextureId);
	glDeleteLists(volumeList[0], 1);
	glDeleteLists(volumeList[1], 1);

}



std::vector<glm::vec4> SpimStack::extractRegistrationPoints(unsigned short threshold) const
{
	assert(volume);


	std::vector<vec4> points;

	
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
					vec4 pt(coord * DIMENSIONS, float(val)/std::numeric_limits<unsigned short>::max());
					points.push_back(pt); 
				}

			}

		}
	}

	float relSize = (float)points.size() / (width*height*depth);
	std::cout << "[Stack] Reconstructed " << points.size() << "(" << relSize << ") points.\n";

	return points;
}


AABB&& SpimStack::getBBox() const
{
	AABB bbox;
	vec3 vol = DIMENSIONS * vec3(width, height, depth);
	bbox.min = vec3(0.f); // -vol * 0.5f;
	bbox.max = vol;// *0.5f;
	
	return std::move(bbox);
}

glm::vec3 SpimStack::getCentroid() const
{
	glm::vec3 center = DIMENSIONS * vec3(width, height, depth) * 0.5f;
	glm::vec4 c = transform * glm::vec4(center, 1.f);
	
	return glm::vec3(c);
}

void SpimStack::move(const glm::vec3& delta)
{
	transform = glm::translate(transform, delta);
}

void SpimStack::rotate(float d)
{
	transform = glm::rotate(transform, d, glm::vec3(0, 1, 0));
}

void SpimStack::drawSlices(Shader* s, const glm::vec3& view) const
{
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_3D, volumeTextureId);
	
	s->setUniform("volumeTexture", 0);

	const glm::vec3 aview = glm::abs(view);

	
	s->setUniform("viewDir", aview);
	
	glPushAttrib(GL_ENABLE_BIT);
	glDisable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	// pick the best slicing based on the current view vector
	if (aview.x > aview.y)
	{
		if (aview.x > aview.z)
		{
			s->setUniform("sliceWeight", 1.f / width);
			drawXPlanes(view);
		}
		else
		{
			s->setUniform("sliceWeight", 1.f / depth);
			drawZPlanes(view);
		}
	}
	else
	{
		if (aview.y > aview.z)
		{
			s->setUniform("sliceWeight", 1.f / height);
			drawYPlanes(view);
		}
		else
		{

			s->setUniform("sliceWeight", 1.f / depth);
			drawZPlanes(view);
		}
	}
	

	glPopAttrib();

	glBindTexture(GL_TEXTURE_3D, 0);



}

void SpimStack::loadRegistration(const string& filename)
{
	ifstream file(filename);
	assert(file.is_open());

	if (!file.is_open())
		throw std::runtime_error("Unable to open file \"" + filename + "\"");

	for (int i = 0; i < 16; ++i)
	{
		std::string buffer;
		std::getline(file, buffer);

		int result = sscanf(buffer.c_str(), "m%*2s: %f", &glm::value_ptr(transform)[i]);
	}

	/*

	glm::mat4 T(1.f);
	glm::translate(T, glm::vec3(-(float)width / 2, -(float)height / 2, -(float)depth / 2));

	transform = glm::transpose(glm::inverse(transform)) * T;

	*/

	std::cout << "[SpimPlane] Read transform: " << transform << std::endl;
}

void SpimStack::saveTransform(const std::string& filename) const
{
	ofstream file(filename);
	assert(file.is_open());

	const float* m = glm::value_ptr(transform);

	for (int i = 0; i < 16; ++i)
		file << m[i] << std::endl;

	std::cout << "[SpimPlane] Saved transform to \"" << filename << "\"\n";
}

void SpimStack::loadTransform(const std::string& filename)
{
	ifstream file(filename);
	//assert(file.is_open());

	if (!file.is_open())
	{
		std::cerr << "[SpimPlane] Unable to load transformation from \"" << filename << "\"!\n";
		return;
	}

	float* m = glm::value_ptr(transform);
	for (int i = 0; i < 16; ++i)
		file >> m[i];

	std::cout << "[SpimPlane] Read transform: " << transform << " from \"" << filename << "\"\n";
}

void SpimStack::setRotation(float angle)
{
	transform = translate(glm::mat4(1.f), getBBox().getCentroid());
	transform = glm::rotate(transform, angle, vec3(0, 1, 0));
	transform = translate(transform, getBBox().getCentroid() * -1.f);
	
}

void SpimStack::drawZPlanes(const glm::vec3& view) const
{
	glBegin(GL_QUADS);



	if (view.z > 0.f)
	{
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
	}
	else
	{

		for (unsigned int z = depth; z > 0; --z)
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
	}
	glEnd();
}

void SpimStack::drawYPlanes(const glm::vec3& view) const
{
	glBegin(GL_QUADS);

	if (view.y > 0.f)
	{
		for (unsigned int y = 0; y < height; ++y)
		{
			float yf = (float)y  * DIMENSIONS.y;
			float yn = (float)y / height;

			const float w = width * DIMENSIONS.x;
			const float d = depth * DIMENSIONS.z;


			glTexCoord3f(0, yn, 0);
			glVertex3f(0, yf, 0);

			glTexCoord3f(1, yn, 0);
			glVertex3f(w, yf, 0);

			glTexCoord3f(1, yn, 1);
			glVertex3f(w, yf, d);

			glTexCoord3f(0, yn, 1);
			glVertex3f(0, yf, d);

		}
	}
	else
	{
		for (unsigned int y = height; y > 0; --y)
		{
			float yf = (float)y  * DIMENSIONS.y;
			float yn = (float)y / height;

			const float w = width * DIMENSIONS.x;
			const float d = depth * DIMENSIONS.z;


			glTexCoord3f(0, yn, 0);
			glVertex3f(0, yf, 0);

			glTexCoord3f(1, yn, 0);
			glVertex3f(w, yf, 0);

			glTexCoord3f(1, yn, 1);
			glVertex3f(w, yf, d);

			glTexCoord3f(0, yn, 1);
			glVertex3f(0, yf, d);

		}

	}



	glEnd();

}

void SpimStack::drawXPlanes(const glm::vec3& view) const
{
	glBegin(GL_QUADS);

	if (view.x > 0.f)
	{
		for (unsigned x = 0; x < width; ++x)
		{
			float xf = (float)x * DIMENSIONS.x;
			float xn = (float)x / width;

			const float h = height * DIMENSIONS.y;
			const float d = depth* DIMENSIONS.z;


			glTexCoord3f(xn, 0, 0);
			glVertex3f(xf, 0.f, 0.f);

			glTexCoord3f(xn, 0, 1);
			glVertex3f(xf, 0.f, d);

			glTexCoord3f(xn, 1, 1);
			glVertex3f(xf, h, d);

			glTexCoord3f(xn, 1, 0);
			glVertex3f(xf, h, 0.f);
		}
	}
	else
	{
		for (unsigned x = width; x > 0; --x)
		{
			float xf = (float)x * DIMENSIONS.x;
			float xn = (float)x / width;

			const float h = height * DIMENSIONS.y;
			const float d = depth* DIMENSIONS.z;


			glTexCoord3f(xn, 0, 0);
			glVertex3f(xf, 0.f, 0.f);

			glTexCoord3f(xn, 0, 1);
			glVertex3f(xf, 0.f, d);

			glTexCoord3f(xn, 1, 1);
			glVertex3f(xf, h, d);

			glTexCoord3f(xn, 1, 0);
			glVertex3f(xf, h, 0.f);
		}
	}


	glEnd();
}