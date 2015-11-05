#include "SpimStack.h"
#include "AABB.h"
#include "Shader.h"
#include "StackRegistration.h"
#include "Histogram.h"

#include <iostream>
#include <cstring>
#include <algorithm>
#include <fstream>

#include <boost/smart_ptr.hpp>


#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/io.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <FreeImage.h>
#include <GL/glew.h>


#include <pcl/common/common.h>
#include <pcl/point_types.h>
#include <pcl/features/feature.h>
#include <pcl/features/intensity_gradient.h>
#include <pcl/features/normal_3d_omp.h>
#include <pcl/features/boundary.h>
#include <pcl/search/kdtree.h>

#include <extrema.h>

/*
#include <pcl/registration/icp.h>
#include <pcl/registration/transformation_estimation.h>
*/

#include <opencv2/calib3d.hpp>
#include <opencv2/core.hpp>
#include <opencv2/opencv.hpp>

using namespace std;
using namespace glm;

// voxel dimensions in microns
static const vec3 DEFAULT_DIMENSIONS(0.625, 0.625, 3);

SpimStack::SpimStack(const string& name, unsigned int subsampleSteps) : filename(name), dimensions(DEFAULT_DIMENSIONS), width(0), height(0), depth(0), volume(nullptr), volumeTextureId(0), transform(1.f), enabled(true)
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

	
	if (subsampleSteps> 0)
	{
		for (int s = 0; s < subsampleSteps; ++s)
			this->subsample(false);
	}

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


void SpimStack::subsample(bool updateTexture)
{
	assert(volume);


	dimensions *= vec3(2.f, 2.f, 1.f);
	
	std::cout << "[Spimstack] Subsampling to " << width / 2 << "x" << height / 2 << "x" << depth << std::endl;

	unsigned short* newData = new unsigned short[(width/2)*(height/2)*depth];
	
	for (unsigned int z = 0; z < depth; ++z)
	{
		for (unsigned int x = 0, nx = 0; x < width;  x+=2, ++nx)
		{
			for (unsigned int y = 0, ny = 0; y < height; y += 2, ++ny)
			{
				//std::cout << "x y z " << x << "," << y << "," << z << " -> " << (x * 2) << "," << (y * 2) << "," << z << std::endl;


				const unsigned short val = volume[x + y*width + z*width*height];
				newData[nx + ny*width / 2 + z*width / 2 * height / 2] = val;

			}
		}
	}



	delete volume;
	volume = newData;
	width /= 2;
	height /= 2;

	if (updateTexture)
	{
		if (!glIsTexture(volumeTextureId))
			glGenTextures(1, &volumeTextureId);

		glBindTexture(GL_TEXTURE_3D, volumeTextureId);
		glTexImage3D(GL_TEXTURE_3D, 0, GL_R16I, width, height, depth, 0, GL_RED_INTEGER, GL_UNSIGNED_SHORT, volume);
	}
}

/*
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
					vec4 pt(coord * dimensions, float(val) / std::numeric_limits<unsigned short>::max());
					points.push_back(pt); 
				}

			}

		}
	}

	float relSize = (float)points.size() / (width*height*depth);
	std::cout << "[Stack] Reconstructed " << points.size() << "(" << relSize << ") points.\n";

	return points;
}
*/

AABB SpimStack::getBBox() const
{
	AABB bbox;
	vec3 vol = dimensions * vec3(width, height, depth);
	bbox.min = vec3(0.f); // -vol * 0.5f;
	bbox.max = vol;// *0.5f;
	
	return std::move(bbox);
}

glm::vec3 SpimStack::getCentroid() const
{
	glm::vec3 center = dimensions * vec3(width, height, depth) * 0.5f;
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

	float sWeight = 1.f;

	// pick the best slicing based on the current view vector
	if (aview.x > aview.y)
	{
		if (aview.x > aview.z)
		{
			s->setUniform("sliceWeight", 1.f / width);
			sWeight = 1.f / width;
			drawXPlanes(view);
		}
		else
		{
			s->setUniform("sliceWeight", 1.f / depth);
			sWeight = 1.f / depth;
			drawZPlanes(view);
		}
	}
	else
	{
		if (aview.y > aview.z)
		{
			s->setUniform("sliceWeight", 1.f / height);
			sWeight = 1.f / height;
			drawYPlanes(view);
		}
		else
		{

			s->setUniform("sliceWeight", 1.f / depth);
			sWeight = 1.f / depth;
			drawZPlanes(view);
		}
	}
	
	/*
	std::cout << "[Debug] viewDir: " << aview << std::endl;
	std::cout << "[Debug] sweight: " << sWeight << std::endl;
	*/

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

	std::cout << "[SpimStack] Saved transform to \"" << filename << "\"\n";
}

void SpimStack::loadTransform(const std::string& filename)
{
	ifstream file(filename);
	//assert(file.is_open());

	if (!file.is_open())
	{
		std::cerr << "[SpimStack] Unable to load transformation from \"" << filename << "\"!\n";
		return;
	}

	float* m = glm::value_ptr(transform);
	for (int i = 0; i < 16; ++i)
		file >> m[i];

	std::cout << "[SpimStack] Read transform: " << transform << " from \"" << filename << "\"\n";
}

void SpimStack::applyTransform(const glm::mat4& t)
{
	transform = t * transform;
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
			float zf = (float)z * dimensions.z;
			float zn = (float)z / depth;

			const float w = width * dimensions.x;
			const float h = height * dimensions.y;

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
			float zf = (float)z * dimensions.z;
			float zn = (float)z / depth;

			const float w = width * dimensions.x;
			const float h = height * dimensions.y;

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
			float yf = (float)y  * dimensions.y;
			float yn = (float)y / height;

			const float w = width * dimensions.x;
			const float d = depth * dimensions.z;


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
			float yf = (float)y  * dimensions.y;
			float yn = (float)y / height;

			const float w = width * dimensions.x;
			const float d = depth * dimensions.z;


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
			float xf = (float)x * dimensions.x;
			float xn = (float)x / width;

			const float h = height * dimensions.y;
			const float d = depth* dimensions.z;


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
			float xf = (float)x * dimensions.x;
			float xn = (float)x / width;

			const float h = height * dimensions.y;
			const float d = depth* dimensions.z;


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


vector<vec4> SpimStack::extractTransformedPoints() const
{
	vector<vec4> points;
	points.reserve(width*height*depth);

	for (unsigned int z = 0; z < depth; ++z)
	{
		for (unsigned int x = 0; x < width; ++x)
		{
			for (unsigned int y = 0; y < height; ++y)
			{
				const unsigned short val = volume[x + y*width + z*width*height];
				vec3 coord(x, y, z);
				vec4 point(coord * dimensions, 1.f);

				// transform to world space
				point = transform * point;
				point.w = float(val) / std::numeric_limits<unsigned short>::max();
				points.push_back(point);
			}

		}
	}

	//cout << "[Stack] Extracted " << points.size() << " points (" << (float)points.size() / (width*height*depth) * 100 << "%)" << std::endl;

 	return std::move(points);
}

vector<vec4> SpimStack::extractTransformedPoints(const SpimStack* clip, const Threshold& t) const
{
	vector<vec4> points;
	points.reserve(width*height*depth);


	mat4 invMat = inverse(clip->transform);
	const AABB bbox = clip->getBBox();

	for (unsigned int z = 0; z < depth; ++z)
	{
		for (unsigned int x = 0; x < width; ++x)
		{
			for (unsigned int y = 0; y < height; ++y)
			{
				const unsigned short val = volume[x + y*width + z*width*height];
				
				if (val >= t.min && val <= t.max)
				{



					vec3 coord(x, y, z);
					vec4 point(coord * dimensions, 1.f);

					// transform to world space
					point = transform * point;

					// transform to the other's clip space
					vec4 pt = invMat * point;
					if (bbox.isInside(vec3(pt)))
					{
						point.w = float(val) / std::numeric_limits<unsigned short>::max();
						points.push_back(point);
					}
				}
			}

		}
	}

	//cout << "[Stack] Extracted " << points.size() << " points (" << (float)points.size() / (width*height*depth) * 100 << "%)" << std::endl;

	return std::move(points);
}

void SpimStack::extractTransformedFeaturePoints(const Threshold& t, ReferencePoints& result) const
{
	using namespace pcl;
	


	std::cout << "[Stack] Calculating 3D gradient ... ";

	// extract points based on value
	int bufferDims[] = { width, height, depth };
	float filterCoefficients[] = { 1.f, 1.f, 1.f };
	int borderLengths[] = { 0, 0, 0 };
	recursiveFilterType filterType = GAUSSIAN_DERICHE;
	
	boost::scoped_array<float> gradients(new float[width*height*depth]);

	if (!Extract_Gradient_Maxima_3D(volume, USHORT, gradients.get(), FLOAT, bufferDims, borderLengths, filterCoefficients, filterType))
	{
		std::cerr << "failed with error!\n";

		std::cout << "[Stack] Copying data to gradients ... ";
		for (size_t i = 0; i < width*height*depth; ++i)
		{
			float v = t.getRelativeValue(volume[i]);

			gradients[i] = v * numeric_limits<unsigned short>::max();
			gradients[i] = volume[i];

		}

	}
	std::cout << "done.\n";


	vector<vec3> normals = this->calculateVolumeNormals();
	

	result.points.clear();
	result.points.reserve(width*height*depth);
	result.normals.clear();
	result.normals.reserve(width*height*depth);

	

	// extract points based on gradient
	const float gradientThreshold = 0.28f;
	std::cout << "[Stack] Extracking points between [" << (int)t.min << "->" << (int)t.max << ") ... ";

	for (unsigned int z = 0; z < depth; ++z)
	{
		for (unsigned int x = 0; x < width; ++x)
		{
			for (unsigned int y = 0; y < height; ++y)
			{
				const unsigned int index = x + y*width + z*width*height;

				float g = gradients[index];
				//if (abs(g) < gradientThreshold)
				
				unsigned short val = volume[index];

				if (val >= t.min && val <= t.max)				
				{
					result.normals.push_back(normals[index]);

					vec3 coord(x, y, z);
					vec4 point(coord * dimensions, 1.f);

					// transform to world space
					point = transform * point;
					point.w = g;
					
					result.points.push_back(point);
				}
			}
		}
	}

	cout << "done.\n";



	std::cout << "[Stack] Extracted " << result.points.size() << " points (" << (float)result.points.size() / (width*height*depth) << ")\n";


	/*
	// extract points based on threshold
	std::cout << "[Stack] Extracking points within [" << (int)t.min << " -> " << (int)t.max << "] ... \n";
	PointCloud<PointXYZI>::Ptr points(new PointCloud<PointXYZI>);
	points->reserve(width*height*depth);

	for (unsigned int z = 0; z < depth; ++z)
	{
		for (unsigned int x = 0; x < width; ++x)
		{
			for (unsigned int y = 0; y < height; ++y)
			{
				const unsigned char val = gradients[x + y*width + z*width*height];

				if (val >= 10)
				{

					vec3 coord(x, y, z);
					vec4 point(coord * dimensions, 1.f);

					// transform to world space
					point = transform * point;
					point.w = float(val) / std::numeric_limits<unsigned short>::max();
				

					//float fval = float(val - t.min) / float(t.max - t.min);
					float fval = 1.f;


					PointXYZI p(fval);
					p.x = point.x;
					p.y = point.y;
					p.z = point.z;
					
					points->push_back(p);				
				}
			}

		}
	}

	std::cout << "[Stack] Extracted " << points->size() << " points (" << (float)points->size() / (width*height*depth) * 100 << "%)" << std::endl;


	
	// create KdTree for searches
	std::cout << "[Stack] Creating Kd Search tree ... ";
	search::KdTree<PointXYZI>::Ptr tree(new search::KdTree<PointXYZI>);
	std::cout << "done.\n";



	std::cout << "[Stack] Extracting hull ";
	tree->setInputCloud(points);
	
	PointCloud<PointXYZI>::Ptr hull(new PointCloud<PointXYZI>);
	
	size_t oldSize = points->size();
	int counter = 0;

	auto it = points->begin();
	while (it != points->end())	
	{
		const PointXYZI& center = *it;
		float radius = 2.0f;

		vector<int> indices;
		vector<float> sqDistances;

		tree->radiusSearch(center, radius, indices, sqDistances, 20);

		// if we have few neighbours we are in the shell and keep the point
		if (indices.size() < 10)
			++it;
		else
			it = points->erase(it);


		++counter;
		if (counter == 1000000)
		{
			std::cout << ".";
			counter = 0;
		}
	}

	std::cout << " done. Reduced to " << points->size() << " points (" << (float)points->size() / (float)oldSize << ")\n";


	
	
	std::cout << "[Stack] Calculating normals ... ";
	PointCloud<Normal>::Ptr  normals(new PointCloud<Normal>);

	NormalEstimationOMP<PointXYZI, Normal> ne;
	ne.setInputCloud(points);
	ne.setSearchMethod(tree);
	ne.setRadiusSearch(3);

	ne.compute(*normals);

	std::cout << "done.\n";
	

	std::cout << "[Stack] Writing registration points ... ";
	result.points.clear();
	result.points.resize(points->size());
	for (size_t i = 0; i < points->size(); ++i)
	{
		const PointXYZI& p = points->at(i);
		result.points[i] = vec4(p.x, p.y, p.z, p.intensity);
	}


	result.normals.clear();
	result.normals.resize(normals->size());
	for (size_t i = 0; i < normals->size(); ++i)
	{
		const Normal& n = normals->at(i);
		result.normals[i] = vec3(n.normal_x, n.normal_y, n.normal_z);
	}

	assert(result.normals.size() == result.points.size());

	std::cout << "done.\n";
	*/

}


Threshold SpimStack::getLimits() const
{
	assert(volume);
	Threshold t;

	t.max = 0;
	t.min = numeric_limits<unsigned short>::max();

	for (unsigned int i = 0; i < width*height*depth; ++i)
	{
		const unsigned short& v = volume[i];
		t.max = std::max(t.max, v);
		t.min = std::min(t.min, v);
	}

	return std::move(t);
}

std::vector<glm::vec3> SpimStack::calculateVolumeNormals() const
{
	std::cout << "[Stack] Calculating volume normals ";

	vector<vec3> normals(width*height*depth);

	for (unsigned int z = 0; z < depth; ++z)
	{
		for (unsigned int x = 0; x < width; ++x)
		{
			normals[x + z*width*height].y = 0.f;
			for (unsigned int y = 1; y < height; ++y)
			{
				short v = volume[x + y*width + z*width*height] - volume[x + (y-1)*width + z*width*height];
				normals[x + y*width + z*width*height].y = float(v) / numeric_limits<short>::max();
			}
		}
	}

	cout << ".";

	for (unsigned int z = 0; z < depth; ++z)
	{
		for (unsigned int y = 0; y < height; ++y)
		{
			normals[y*width + z*width*height].x = 0.f;
			for (unsigned int x = 1; x < width; ++x)
			{
				short v = volume[x + y*width + z*width*height] - volume[x -1 + y*width + z*width*height];
				normals[x + y*width + z*width*height].x = float(v) / numeric_limits<short>::max();
			}
		}
	}

	cout << ".";

	for (unsigned int x = 0; x < width; ++x)
	{
		for (unsigned int y = 0; y < height; ++y)
		{
			normals[x + y*width].z = 0.f;
			for (unsigned int z = 1; z < depth; ++z)
			{
				short v = volume[x + y*width + z*width*height] - volume[x + y*width + (z-1)*width*height];
				normals[x + y*width + z*width*height].z = float(v) / numeric_limits<short>::max();
			}
		}
	}

	cout << ".";

	for (size_t i = 0; i < width*height*depth; ++i)
		normals[i] = normalize(normals[i]);
	

	cout << "done.\n";

	return std::move(normals);
}
