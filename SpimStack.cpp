#include "SpimStack.h"
#include "AABB.h"
#include "Shader.h"
#include "StackRegistration.h"
#include "BeadDetection.h"

#include <iostream>
#include <cstring>
#include <algorithm>
#include <fstream>
#include <cstdio>

#include <random>
#include <chrono>

//#define ENABLE_PCL


#include <boost/smart_ptr.hpp>


#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/io.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform2.hpp>

#include <FreeImage.h>
#include <GL/glew.h>

#ifdef ENABLE_PCL
#include <pcl/common/common.h>
#include <pcl/point_types.h>
#include <pcl/features/feature.h>
#include <pcl/features/intensity_gradient.h>
#include <pcl/features/normal_3d_omp.h>
#include <pcl/features/boundary.h>
#include <pcl/search/kdtree.h>

/*
#include <pcl/registration/icp.h>
#include <pcl/registration/transformation_estimation.h>
*/

#endif

#ifdef ENABLE_OPENCV
#include <opencv2/imgproc.hpp>
#include <opencv2/calib3d.hpp>
#include <opencv2/core.hpp>
#include <opencv2/opencv.hpp>
#endif

using namespace std;
using namespace glm;

// voxel dimensions in microns
static const vec3 DEFAULT_DIMENSIONS(0.625, 0.625, 3);

SpimStack::SpimStack() : filename(""), dimensions(DEFAULT_DIMENSIONS), width(0), height(0), depth(0), volumeTextureId(0)
{
	volumeList[0] = 0;
	volumeList[1] = 0;

#ifndef NO_GRAPHICS
	glGenTextures(1, &volumeTextureId);
	cout << "[Stack] Created new texture id: " << volumeTextureId << endl;
	glBindTexture(GL_TEXTURE_3D, volumeTextureId);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glBindTexture(GL_TEXTURE_3D, 0);
#endif


}

SpimStack::~SpimStack()
{
#ifndef NO_GRAPHICS
	glDeleteTextures(1, &volumeTextureId);
	glDeleteLists(volumeList[0], 1);
	glDeleteLists(volumeList[1], 1);
#endif
}

void SpimStack::update()
{
	updateStats();
	updateTexture();
}

void SpimStack::load(const std::string& file)
{
	if (!filename.empty())
		throw std::runtime_error("File already loaded: \"" + filename + "\"!");
	
	filename = file;


	if (filename.find(".bin") == string::npos)
		loadImage(filename);
	else
	{

		string s = filename.substr(filename.find_last_of("_"));

		cout << "[Debug] " << s << endl;
		ivec3 res;
#ifdef _WIN32
		assert(sscanf_s(s.c_str(), "_%dx%dx%d.bin", &res.x, &res.y, &res.z) == 3);
#else
		assert(sscanf(s.c_str(), "_%dx%dx%d.bin", &res.x, &res.y, &res.z) == 3);
#endif
		cout << "[Debug] Reading binary volume with resolution " << res << endl;

		loadBinary(filename, res);
	}

	updateTexture();
	updateStats();
}

void SpimStack::save(const std::string& file)
{
	if (filename.find(".bin") == string::npos)
		saveImage(file);
	else
		saveBinary(file);
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

glm::vec3 SpimStack::getCentroid() const
{
	glm::vec3 center = dimensions * vec3(width, height, depth) * 0.5f;
	glm::vec4 c = getTransform() * glm::vec4(center, 1.f);
	
	return glm::vec3(c);
}

glm::vec3 SpimStack::getWorldPosition(const glm::ivec3& coordinate) const
{
	/*
	if (coordinate.x >= 0 && coordinate.x < width &&
		coordinate.y >= 0 && coordinate.y < height &&
		coordinate.z >= 0 && coordinate.z < height)
	*/
		
	// apply non-uniform pixel scaling
	vec4 pt(vec3(coordinate) * dimensions, 1.f);
	pt = getTransform() * pt;
		
	return vec3(pt);
}



glm::vec3 SpimStack::getWorldPosition(const unsigned int index) const
{
	glm::ivec3 sc = getStackCoords(index);
	return getWorldPosition(sc);
}

glm::ivec3 SpimStack::getStackCoords(size_t index) const
{
	assert(index < width*height*depth);

	// see: http://stackoverflow.com/questions/10903149/how-do-i-compute-the-linear-index-of-a-3d-coordinate-and-vice-versa

	glm::ivec3 coords;

	coords.x = index % (width + 1);
	index /= (width + 1);

	coords.y = index % (height + 1);
	index /= (height + 1);

	coords.z = index;

	return coords;
}

glm::ivec3 SpimStack::getStackVoxelCoords(const glm::vec4& worldCoords) const
{
	// get stack coordinates
	glm::vec4 stackCoords = getInverseTransform() * worldCoords;
	stackCoords /= glm::vec4(dimensions, 1.0);

	return glm::ivec3(stackCoords.x, stackCoords.y, stackCoords.z);
}



void SpimStack::drawSlices(Shader* s, const glm::vec3& view) const
{
#ifndef NO_GRAPHICS
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_3D, volumeTextureId);
	
	s->setUniform("volumeTexture", 0);

	const glm::vec3 aview = glm::abs(view);

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

			//s->setUniform("sliceWeight", 1.f / depth);
			s->setUniform("sliceWeight", 0.02f);
			drawZPlanes(view);
		}
	}
	

	glPopAttrib();

	glBindTexture(GL_TEXTURE_3D, 0);

#endif

}

void SpimStack::drawZSlices() const
{
#ifndef NO_GRAPHICS
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_3D, volumeTextureId);

	glPushAttrib(GL_ENABLE_BIT);
	glDisable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	glColor3f(1, 1, 1);
	drawZPlanes(glm::vec3(0, 0, 1));

	glPopAttrib();
	glBindTexture(GL_TEXTURE_3D, 0);

#endif
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

		mat4 T(1.f);

#ifdef _WIN32
		int result = sscanf_s(buffer.c_str(), "m%*2s: %f", &glm::value_ptr(T)[i]);
#else
		int result = sscanf(buffer.c_str(), "m%*2s: %f", &glm::value_ptr(T)[i]);
#endif

		setTransform(T);

	}

	/*

	glm::mat4 T(1.f);
	glm::translate(T, glm::vec3(-(float)width / 2, -(float)height / 2, -(float)depth / 2));

	transform = glm::transpose(glm::inverse(transform)) * T;

	*/

	std::cout << "[SpimPlane] Read transform: " << getTransform() << std::endl;
}

#ifndef NO_GRAPHICS
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
#endif

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
				//const unsigned short val = volume[x + y*width + z*width*height];
				vec3 coord(x, y, z);
				vec4 point(coord * dimensions, 1.f);

				// transform to world space
				point = getTransform() * point;
				//point.w = float(val) / std::numeric_limits<unsigned short>::max();
				point.w = (float)getRelativeValue(getIndex(x, y, z));
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


	const AABB bbox = clip->getBBox();

	for (unsigned int z = 0; z < depth; ++z)
	{
		for (unsigned int x = 0; x < width; ++x)
		{
			for (unsigned int y = 0; y < height; ++y)
			{
			
				double v = getValue(getIndex(x, y, z));
				if (v >= t.min && v <= t.max)
				{



					vec3 coord(x, y, z);
					vec4 point(coord * dimensions, 1.f);

					// transform to world space
					point = getTransform() * point;

					// transform to the other's clip space
					vec4 pt = getInverseTransform() * point;
					if (bbox.isInside(vec3(pt)))
					{
						point.w = (float)getRelativeValue(getIndex(x, y, z));
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
#ifdef ENABLE_PCL


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


#else
	std::cerr << "[Error] ExtractTransformedFeaturePoints disabled, due to missing PCL!\n";
#endif
}


Threshold SpimStack::getLimits() const
{
	Threshold t;

	t.max = 0;
	t.min = numeric_limits<double>::max();
	t.mean = 0;

	const size_t count = width*height*depth;

	for (size_t i = 0; i < count; ++i)
	{
		//const unsigned short& v = volume[i];
		const double v = getValue(i);
		t.max = std::max(t.max, v);
		t.min = std::min(t.min, v);
	
		t.mean += (unsigned int)v;
	}

	t.mean /= count;

	double variance = 0;
	for (size_t i = 0; i < count; ++i)
	{
		double v = (double)getValue(i);
		variance = variance + (v - t.mean)*(v - t.mean);
	}
	
	variance /= count;
	t.stdDeviation = ::sqrt((double)variance);

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
				//short v = volume[x + y*width + z*width*height] - volume[x + (y-1)*width + z*width*height];
				float dy = (float)(getRelativeValue(getIndex(x, y, z)) - getRelativeValue(getIndex(x, y - 1, z)));
				normals[x + y*width + z*width*height].y = dy;
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
				
				//short v = volume[x + y*width + z*width*height] - volume[x -1 + y*width + z*width*height];
				float dx = (float)(getRelativeValue(getIndex(x, y, z)) - getRelativeValue(getIndex(x - 1, y, z)));
				normals[x + y*width + z*width*height].x = dx;
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
				//short v = volume[x + y*width + z*width*height] - volume[x + y*width + (z-1)*width*height];
				float dz = (float)(getRelativeValue(getIndex(x, y, z)) - getRelativeValue(getIndex(x, y, z - 1)));
				normals[x + y*width + z*width*height].z = dz; // float(v) / numeric_limits<short>::max();
			}
		}
	}

	cout << ".";

	for (size_t i = 0; i < width*height*depth; ++i)
		normals[i] = normalize(normals[i]);
	

	cout << "done.\n";

	return std::move(normals);
}


vector<size_t> SpimStack::calculateHistogram(const Threshold& t) const
{
	vector<size_t> histogram((size_t)std::ceil(t.getSpread()));
	
	
	for (size_t i = 0; i < width*height*depth; ++i)
	{
		/*
		int j = (int)volume[i] - (int)t.min;
		if (j >= 0 && j < histogram.size())
			++histogram[j];

		*/
		int j = (int)floor(getValue(i) - t.min);
		if (j >= 0 && j < histogram.size())
			++histogram[j];

	}
	

	return std::move(histogram);

}

/*
vector<Hourglass> SpimStack::detectHourglasses() const
{
	using namespace cv;
	

	// combine the circles on different planes to hourglasses
	vector<Hourglass> result;


	// convert each plane of the volume into an opencv matrix
	for (unsigned int i = 0; i < depth; ++i)
	{
		Mat plane = getImagePlane(i);
		Mat plane8bit;

		// convert image to 8bit gray
		plane.convertTo(plane8bit, CV_8U);

		GaussianBlur(plane8bit, plane8bit, Size(9, 9), 2, 2);
		vector<Vec3f> circles;

		
		HoughCircles(plane8bit, circles, CV_HOUGH_GRADIENT, 1, plane8bit.rows / 8, 6, 30, 0, 0);
	
		cout << "[Beads] Detected " << circles.size() << " circles in plane " << i << endl;


		char filename[256];
		sprintf(filename, "e:/temp/circles_%d.png", i);
		

		Mat imgOut;
		cvtColor(plane8bit, imgOut, CV_GRAY2RGB);
		
		for (size_t i = 0; i < circles.size(); ++i)
		{
			Point center(cvRound(circles[i][0]), cvRound(circles[i][1]));
			int radius = cvRound(circles[i][2]);
			// circle center
			circle(imgOut, center, 3, Scalar(0, 255, 0), -1, 8, 0);
			// circle outline
			circle(imgOut, center, radius, Scalar(0, 0, 255), 3, 8, 0);

		}	
		
		
		imwrite(filename, imgOut);



		cout << "[Beads] Assigning circles to hourglasses ... ";
		for (size_t c = 0; c < circles.size(); ++c)
		{
			const vec2 axis(circles[c][0], circles[c][1]);
			const float radius = circles[c][2];


			if (result.empty())
			{
				Hourglass h;
				h.centerAxis = axis;
				h.circles.push_back(vec2(i, radius));
		
				result.push_back(h);
			}
			else
			{
				size_t bestFit = 0;
				float maxDist = numeric_limits<float>::max();
				// find a fitting hourglass shape if it exists
				for (size_t h = 0; h < result.size(); ++h)
				{
					vec2 d = axis - result[h].centerAxis;
					float d2 = dot(d, d);
					if (d2 < maxDist)
					{
						bestFit = h;
						maxDist = d2;
					}
				}

				Hourglass& h = result[bestFit];
				
				h.circles.push_back(vec2(i, radius));
			}


		}
		cout << "done.\n";

	}

	cout << "[Beads] Found " << result.size() << " hourglasses.\n";





	return std::move(result);

}
*/

/*
cv::Mat SpimStack::getImagePlane(unsigned int z) const
{
	using namespace cv;

	assert(z < depth);

	Mat result(width, height, CV_16U, Scalar(0));
	for (unsigned int x = 0; x < width; ++x)
	{
		for (unsigned int y = 0; y < height; ++y)
		{
			const unsigned int index = x + y*width + z*width*height;
			unsigned short val = volume[index];
			
			result.at<unsigned short>(x,y) = val;
		}
	}
	

	return std::move(result);
}
*/

AABB SpimStack::getTransformedBBox() const
{
	using namespace glm;
	using namespace std;

	AABB result;
	vector<vec3> verts = getBBox().getVertices();
	for (size_t i = 0; i < verts.size(); ++i)
	{
		vec4 v = this->getTransform() * vec4(verts[i], 1.f);

		if (i == 0)
			result.reset(vec3(v));
		else
			result.extend(vec3(v));
	}


	return std::move(result);
}


void SpimStack::updateStats()
{
	cout << "[Stack] Updating stats ... ";
	maxVal = 0.f;
	minVal = std::numeric_limits<float>::max();
	for (size_t i = 0; i < width*height*depth; ++i)
	{
		double val = getValue(i);
		maxVal = std::max(maxVal, val);
		minVal = std::min(minVal, val);
	}
	cout << "done; range: " << minVal << "-" << maxVal << endl;

	cout << "[Stack] Calculating bbox ... ";
	vec3 vol = dimensions * vec3(width, height, depth);
	bbox.min = vec3(0.f); // -vol * 0.5f;
	bbox.max = vol;// *0.5f;
	cout << "done.\n";
}


double SpimStack::getSample(const glm::vec3& worldCoords)
{

	ivec3 stackCoords = getStackVoxelCoords(worldCoords);
	double result = 0.f;

	// clamp stack coords
	if (stackCoords.x >= 0 && stackCoords.x < width &&
		stackCoords.y >= 0 && stackCoords.y < height &&
		stackCoords.z >= 0 && stackCoords.z < depth)
	{
		size_t index = getIndex(stackCoords);
		result = getValue(index);
	}
	
	return result;
}

SpimStackU16::SpimStackU16() : SpimStack(), volume(nullptr)
{
}

SpimStackU16::~SpimStackU16()
{
	delete[] volume;
}




void SpimStackU16::loadBinary(const std::string& filename, const glm::ivec3& res)
{
	width = res.x;
	height = res.y;
	depth = res.z;

	volume = new unsigned short[width*height*depth];

	std::ifstream file(filename, ios::binary);
	assert(file.is_open());

	file.read(reinterpret_cast<char*>(volume), width*height*depth*sizeof(unsigned short));


	cout << "[Stack] Loaded binary volume: " << width << "x" << height << "x" << depth << endl;

}

void SpimStackU16::loadImage(const std::string& filename)
{
	FIMULTIBITMAP* fmb = FreeImage_OpenMultiBitmap(FIF_TIFF, filename.c_str(), FALSE, TRUE);

	if (!fmb)
		throw std::runtime_error("Unable to open image \"" + filename + "\"!");

	assert(fmb);

	// get the dimensions
	depth = FreeImage_GetPageCount(fmb);
	bool initialized = false;


	for (unsigned int z = 0; z < depth; ++z)
	{
		FIBITMAP* bm = FreeImage_LockPage(fmb, z);

		int w = FreeImage_GetWidth(bm);
		int h = FreeImage_GetHeight(bm);
		
		unsigned int bpp = FreeImage_GetBPP(bm);
		assert(bpp == 16);
		

		if (!initialized)
		{
			initialized = true;
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

}

void SpimStackU16::saveBinary(const std::string& filename)
{
	std::ofstream file(filename, ios::binary);
	assert(file.is_open());

	const size_t size = width*height*depth*sizeof(unsigned short);

	file.write(reinterpret_cast<const char*>(volume), size);

	cout << "[Stack] Saved " << size << " bytes to binary file \"" << filename << "\".\n";
}

void SpimStackU16::saveImage(const std::string& filename)
{
	FIMULTIBITMAP* fmb = FreeImage_OpenMultiBitmap(FIF_TIFF, filename.c_str(), TRUE, FALSE);
	assert(fmb);


	for (unsigned int z = 0; z < depth; ++z)
	{
		FIBITMAP* bm = FreeImage_AllocateT(FIT_UINT16, width, height, 16);
		assert(bm);

		BYTE* data = FreeImage_GetBits(bm);
		memcpy(data, &volume[width*height*z], width*height*sizeof(unsigned short));
		
		FreeImage_AppendPage(fmb, bm);

		FreeImage_Unload(bm);
	}

	FreeImage_CloseMultiBitmap(fmb);
}



void SpimStackU16::subsample(bool updateTextureData)
{
	assert(volume);


	dimensions *= vec3(2.f, 2.f, 1.f);

	std::cout << "[Spimstack] Subsampling to " << width / 2 << "x" << height / 2 << "x" << depth << std::endl;

	unsigned short* newData = new unsigned short[(width / 2)*(height / 2)*depth];

	for (unsigned int z = 0; z < depth; ++z)
	{
		for (unsigned int x = 0, nx = 0; x < width; x += 2, ++nx)
		{
			for (unsigned int y = 0, ny = 0; y < height; y += 2, ++ny)
			{
				//std::cout << "x y z " << x << "," << y << "," << z << " -> " << (x * 2) << "," << (y * 2) << "," << z << std::endl;

				const unsigned short val = volume[x + y*width + z*width*height];
				newData[nx + ny*width / 2 + z*width / 2 * height / 2] = val;

			}
		}
	}

	delete[] volume;
	volume = newData;
	width /= 2;
	height /= 2;

	if (updateTextureData)
		updateTexture();
}

void SpimStackU16::updateTexture()
{
#ifndef NO_GRAPHICS
	cout << "[Stack] Updating 3D texture ... ";
	glBindTexture(GL_TEXTURE_3D, volumeTextureId);
	assert(volume);
	glTexImage3D(GL_TEXTURE_3D, 0, GL_R16UI, width, height, depth, 0, GL_RED_INTEGER, GL_UNSIGNED_SHORT, volume);
	cout << "done.\n";
#else
	cout << "[Stack] Compiled with NO_GRAPHICS, texture will not be updated.\n";
#endif
}


void SpimStackU16::setContent(const glm::ivec3& res, const void* data)
{
	delete[] volume;

	width = res.x;
	height = res.y;
	depth = res.z;

	filename = "";
	
	volume = new unsigned short[width*height*depth];

	if (data)
		memcpy(volume, data, width*height*depth*sizeof(unsigned short));
	else
		memset(volume, 0, width*height*depth*sizeof(unsigned short));
	
	updateTexture();
	
	if (data)
		updateStats();
	else
	{
		maxVal = 0; 
		minVal = 0;

		vec3 vol = dimensions * vec3(width, height, depth);
		bbox.min = vec3(0.f); // -vol * 0.5f;
		bbox.max = vol;// *0.5f;
	}
}


/// Note: _YOU_ have to make sure that the coordinates are within a valid range!
void SpimStackU16::setSample(const glm::ivec3& pos, double value)
{
	size_t index = getIndex(pos.x, pos.y, pos.z);
	assert(index < width*height*depth);

	volume[index] = static_cast<unsigned short>(value);
}


SpimStackU8::~SpimStackU8()
{
	delete[] volume;
}




void SpimStackU8::loadBinary(const std::string& filename, const glm::ivec3& res)
{
	width = res.x;
	height = res.y;
	depth = res.z;

	volume = new unsigned char[width*height*depth];

	std::ifstream file(filename, ios::binary);
	assert(file.is_open());

	file.read(reinterpret_cast<char*>(volume), width*height*depth*sizeof(unsigned char));


	cout << "[Stack] Loaded binary volume: " << width << "x" << height << "x" << depth << endl;

}




void SpimStackU8::loadImage(const std::string& filename)
{
	FIMULTIBITMAP* fmb = FreeImage_OpenMultiBitmap(FIF_TIFF, filename.c_str(), FALSE, TRUE);

	if (!fmb)
		throw std::runtime_error("Unable to open image \"" + filename + "\"!");

	assert(fmb);

	// get the dimensions
	depth = FreeImage_GetPageCount(fmb);
	bool initialized = false;


	for (unsigned int z = 0; z < depth; ++z)
	{
		FIBITMAP* bm = FreeImage_LockPage(fmb, z);

		int w = FreeImage_GetWidth(bm);
		int h = FreeImage_GetHeight(bm);

		unsigned int bpp = FreeImage_GetBPP(bm);
		assert(bpp == 8);


		if (!initialized)
		{
			initialized = true;
			width = w;
			height = h;
			volume = new unsigned char[width*height*depth];
		}
		else
		{
			assert(width == w);
			assert(height = h);
		}


		unsigned char* bits = reinterpret_cast<unsigned char*>(FreeImage_GetBits(bm));

		size_t offset = width*height*z;
		memcpy(&volume[offset], bits, sizeof(unsigned char)*width*height);

		FreeImage_UnlockPage(fmb, bm, FALSE);
	}

	cout << "[Stack] Loaded image stack: " << width << "x" << height << "x" << depth << endl;


	FreeImage_CloseMultiBitmap(fmb);

}

void SpimStackU8::subsample(bool updateTextureData)
{
	assert(volume);


	dimensions *= vec3(2.f, 2.f, 1.f);

	std::cout << "[Spimstack] Subsampling to " << width / 2 << "x" << height / 2 << "x" << depth << std::endl;

	unsigned char* newData = new unsigned char[(width / 2)*(height / 2)*depth];

	for (unsigned int z = 0; z < depth; ++z)
	{
		for (unsigned int x = 0, nx = 0; x < width; x += 2, ++nx)
		{
			for (unsigned int y = 0, ny = 0; y < height; y += 2, ++ny)
			{
				//std::cout << "x y z " << x << "," << y << "," << z << " -> " << (x * 2) << "," << (y * 2) << "," << z << std::endl;

				const unsigned char val = volume[x + y*width + z*width*height];
				newData[nx + ny*width / 2 + z*width / 2 * height / 2] = val;

			}
		}
	}

	delete[] volume;
	volume = newData;
	width /= 2;
	height /= 2;

	if (updateTextureData)
		updateTexture();
}

void SpimStackU8::updateTexture()
{
#ifndef NO_GRAPHICS
	cout << "[Stack] Updating 3D texture ... ";
	glBindTexture(GL_TEXTURE_3D, volumeTextureId);
	assert(volume);
	glTexImage3D(GL_TEXTURE_3D, 0, GL_R8UI, width, height, depth, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, volume);
	cout << "done.\n";
#endif
}

void SpimStack::addNoise(float amount, double valueRange)
{
#if 0

	if (amount == 0.f)
		return;

	if (amount > 1.f)
		amount = 1.f;

	auto seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
	std::mt19937 rng(seed);
	auto randVal = std::uniform_real_distribution<double>(-valueRange, valueRange);
	
	auto randIdx = std::uniform_real_distribution<unsigned int>(0, getVoxelCount());

	for (unsigned int i = 0; i < (unsigned int)floor(getVoxelCount()*amount); ++i)
	{
		unsigned int idx = randIdx(rng);
		

		//TODO: implement rest ...
	}

#endif
	
}