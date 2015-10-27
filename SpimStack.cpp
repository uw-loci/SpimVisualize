#include "SpimStack.h"
#include "AABB.h"
#include "Shader.h"
#include "nanoflann.hpp"

#include <iostream>
#include <cstring>
#include <algorithm>
#include <fstream>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/io.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <FreeImage.h>
#include <GL/glew.h>

/*
#include <pcl/common/common.h>
#include <pcl/point_types.h>
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


vector<vec4> SpimStack::clipPoints(const vector<vec4>& points) const
{
	// keep only the points that are in this stacks's bbox
	mat4 invMat = inverse(transform);
	const AABB bbox = this->getBBox();

	vector<vec4> clipped;

	for (size_t i = 0; i < points.size(); ++i)
	{
		vec4 pt = invMat * vec4(vec3(points[i]), 1.f);
		if (bbox.isInside(vec3(pt)))
			clipped.push_back(points[i]);
	}


	std::cout << "[Stack] Clipped " << points.size() - clipped.size() << " points.\n";

	return std::move(clipped);
}


// And this is the "dataset to kd-tree" adaptor class:
struct PointCloudAdaptor
{
	const std::vector<glm::vec4>&		points;

	/// The constructor that sets the data set source
	PointCloudAdaptor(const std::vector<glm::vec4>& p) : points(p){ }
	
	// Must return the number of data points
	inline size_t kdtree_get_point_count() const { return points.size(); }

	// Returns the distance between the vector "p1[0:size-1]" and the data point with index "idx_p2" stored in the class:
	inline float kdtree_distance(const float *p1, const size_t idx_p2, size_t /*size*/) const
	{
		vec4 a(p1[0], p1[1], p1[2], p1[3]);
		vec4 delta = a - points[idx_p2];
		return dot(delta, delta);
	}

	// Returns the dim'th component of the idx'th point in the class:
	// Since this is inlined and the "dim" argument is typically an immediate value, the
	//  "if/else's" are actually solved at compile time.
	inline float kdtree_get_pt(const size_t idx, int dim) const
	{
		if (dim == 0) return points[idx].x;
		else if (dim == 1) return points[idx].y;
		else if (dim == 2) return points[idx].z;
		else return points[idx].w;
	}

	// Optional bounding-box computation: return false to default to a standard bbox computation loop.
	//   Return true if the BBOX was already computed by the class and returned in "bb" so it can be avoided to redo it again.
	//   Look at bb.size() to find out the expected dimensionality (e.g. 2 or 3 for point clouds)
	template <class BBOX>
	bool kdtree_get_bbox(BBOX& /*bb*/) const { return false; }

}; // end of PointCloudAdaptor


float SpimStack::alignSingleStep(const SpimStack* reference, glm::mat4& delta, std::vector<glm::vec4>& debugPoints)
{

	// extract transformed points and clip against other bbox
	vector<vec4> referencePoints = reference->extractTransformedPoints();;
	referencePoints = this->clipPoints(referencePoints);

	// if we have no points here, the other cloud must also be empty
	if (referencePoints.empty())
	{
		std::cout << "[Align] No overlapping points remaining, quitting.\n";
		return 0.f;
	}

	vector<vec4> targetPoints = this->extractTransformedPoints();
	targetPoints = reference->clipPoints(targetPoints);



	std::cout << "[Align] Extracted " << referencePoints.size() << " reference and\n";
	std::cout << "[Align]           " << targetPoints.size() << " target points.\n";
	std::cout << "[Align] Overlap:  " << (float)std::min(referencePoints.size(), targetPoints.size()) / std::max(referencePoints.size(), targetPoints.size()) << std::endl;
	/*


	const PointCloudAdaptor refAdaptor(referencePoints);
	const PointCloudAdaptor tgtAdaptor(targetPoints);

	// construct a kd-tree index:
	typedef nanoflann::KDTreeSingleIndexAdaptor<
	nanoflann::L2_Simple_Adaptor<float, PointCloudAdaptor>,
	PointCloudAdaptor,
	4
	> KdTree;

	const int MAX_LEAF = 12;
	KdTree refTree(4, refAdaptor, MAX_LEAF);
	refTree.buildIndex();


	// find the closest points
	vector<size_t> closestReference(targetPoints.size());
	float meanDistance = 0.f;

	std::cout << "[Align] Searching for closest points (O(nlogn)) = O(" << (int)(referencePoints.size()*log((float)targetPoints.size())) / 1000000 << "e6) ... \n";
	for (size_t i = 0; i < targetPoints.size(); ++i)
	{
	// knn search
	const size_t num_results = 1;
	size_t ret_index;
	float out_dist_sqr;
	nanoflann::KNNResultSet<float> resultSet(num_results);
	resultSet.init(&ret_index, &out_dist_sqr);

	const vec4& queryPt = targetPoints[i];

	refTree.findNeighbors(resultSet, &queryPt[0], nanoflann::SearchParams(10));

	closestReference[i] = ret_index;
	meanDistance += sqrtf(out_dist_sqr);
	}

	// calculate mean distance/error
	meanDistance /= closestReference.size();
	std::cout << "[Align] Mean distance before alignment: " << meanDistance << std::endl;
	*/


	// calculate delta transformation
	delta = mat4(1.f);


	/*
	// convert to pcl pointclouds
	pcl::PointCloud<pcl::PointXYZI>::Ptr refCloud(new pcl::PointCloud<pcl::PointXYZI>);
	pcl::PointCloud<pcl::PointXYZI>::Ptr tgtCloud(new pcl::PointCloud<pcl::PointXYZI>);

	std::for_each(targetPoints.begin(), targetPoints.end(), [&tgtCloud](const glm::vec4& v)
	{
	pcl::PointXYZI pt;
	pt.x = v.x;
	pt.y = v.y;
	pt.z = v.z;
	pt.intensity = v.w;

	tgtCloud->push_back(pt);
	});

	std::for_each(referencePoints.begin(), referencePoints.end(), [&refCloud](const glm::vec4& v)
	{
	pcl::PointXYZI pt;
	pt.x = v.x;
	pt.y = v.y;
	pt.z = v.z;
	pt.intensity = v.w;

	refCloud->push_back(pt);
	});


	pcl::IterativeClosestPoint<pcl::PointXYZI, pcl::PointXYZI> icp;
	icp.setInputCloud(tgtCloud);
	icp.setInputTarget(refCloud);
	pcl::PointCloud<pcl::PointXYZI> finalCloud;
	icp.align(finalCloud);
	std::cout << "has converged:" << icp.hasConverged() << " score: " <<
	icp.getFitnessScore() << std::endl;
	std::cout << icp.getFinalTransformation() << std::endl;


	*/


	// convert clouds to opencv
	cv::Mat tgtCloud(1, targetPoints.size(), CV_32FC3);
	cv::Point3f* data = tgtCloud.ptr<cv::Point3f>();
	for (size_t i = 0; i < targetPoints.size(); ++i)
	{
		const glm::vec4& pt = targetPoints[i];
		data[i].x = pt.x;
		data[i].y = pt.y;
		data[i].z = pt.z;
	}
		
	cv::Mat refCloud(1, referencePoints.size(), CV_32FC3);
	data = refCloud.ptr<cv::Point3f>();
	for (size_t i = 0; i < referencePoints.size(); ++i)
	{
		const glm::vec4& pt = referencePoints[i];
		data[i].x = pt.x;
		data[i].y = pt.y;
		data[i].z = pt.z;
	}

	
	cv::Mat transformEstimate(3, 4, CV_32F);
	vector<uchar> outliers;

	try
	{

		double ransacThreshold = 0.1;
		double confidence = 0.9;
		cv::estimateAffine3D(tgtCloud, refCloud, transformEstimate, outliers, ransacThreshold, confidence);

		std::cout << "[Debug] Transform estimate: " << transformEstimate << std::endl;


		// transform points
		mat4 oldTransform = this->transform;

		transform = mat4(1.f);
		for (int i = 0; i < 4; ++i)
		{
			for (int j = 0; j < 3; ++j)
				transform[i][j] = transformEstimate.at<float>(i, j);

			transform[i][3] = 0.f;
		}
		transform[3][3] = 1.f;

		transform *= delta;
	}
	catch (cv::Exception& e)
	{
		std::cout << "[Debug] OpenCV error: " << e.what() << std::endl;
		return 0.f;
	}


	/*
	

	// calculate new error ... ?
	float  meanDistance = 0.f;
	for (size_t i = 0; i < closestReference.size(); ++i)
	{
		vec4 a = targetPoints[i];
		vec4 b = referencePoints[closestReference[i]];
		vec4 d = a - b;

		meanDistance += dot(d, d);
	}
	meanDistance /= closestReference.size();
	std::cout << "[Align] Mean distance after alignment: " << meanDistance << std::endl;

	*/



	debugPoints = referencePoints;
	std::cout << "[Debug] Extracted " << debugPoints.size() << " points.\n";



	return 0.f;
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