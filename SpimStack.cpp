#include "SpimStack.h"

#include <iostream>
#include <cstring>

#include <FreeImage.h>

using namespace std;
using namespace glm;

SpimStack::SpimStack(const string& filename) : width(0), height(0), depth(0), volume(nullptr)
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

	std::cout << "[Stack] Loaded image stack: " << width << "x" << height << "x" << depth << std::endl;


	FreeImage_CloseMultiBitmap(fmb);
}

SpimStack::~SpimStack()
{
	delete volume;
}


vector<vec3> SpimStack::getPointcloud(unsigned short threshold) const
{
	assert(volume);

	// voxel dimensions in microns
	const vec3 DIMENSIONS(0.625, 0.625, 3);

	// find the largest value for scaling
	unsigned short maxVal = 0;
	for (unsigned int i = 0; i < width*height*depth; ++i)
		maxVal = std::max(maxVal, volume[i]);


	vector<vec3> points;
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
					float intensity = (float)val / maxVal;
					points.push_back(coord * DIMENSIONS);
				}

			}

		}
	}

	float relSize = (float)points.size() / (width*height*depth);
	std::cout << "[Stack] Reconstructed " << points.size() << "(" << relSize << ") points.\n";
	
	return points;
}