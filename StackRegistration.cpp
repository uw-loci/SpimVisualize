#include "StackRegistration.h"
#include "nanoflann.hpp"

#include <stdexcept>
#include <iostream>

#include <opencv2/core.hpp>
#include <opencv2/calib3d.hpp>


#include <pcl/point_types.h>
#include <pcl/point_cloud.h>
#include <pcl/registration/transformation_estimation_svd.h>


#include <glm/gtx/io.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <GL/glew.h>

using namespace std;
using namespace glm;

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


void ReferencePoints::trim(const ReferencePoints* smaller)
{
	const PointCloudAdaptor refAdaptor(smaller->points);

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
	vector<size_t> closestIndex(smaller->size(), 0);

	cout << "[Align] Searching for closest points (O(nlogn)) = O(" << (int)(smaller->points.size()*log((float)this->points.size())) / 1000000 << "e6) ... \n";
	for (size_t i = 0; i < points.size(); ++i)
	{
		// knn search
		const size_t num_results = 1;
		size_t ret_index;
		float out_dist_sqr;
		nanoflann::KNNResultSet<float> resultSet(num_results);
		resultSet.init(&ret_index, &out_dist_sqr);

		const vec4& queryPt = this->points[i];

		refTree.findNeighbors(resultSet, &queryPt[0], nanoflann::SearchParams(10));

		closestIndex[ret_index] = i;;
	}

	
	vector<vec4> temp(points);
	points.resize(closestIndex.size());

	for (size_t i = 0; i < closestIndex.size(); ++i)
	{
		points[i] = temp[closestIndex[i]];
	}


	std::cout << "[Align] Done reshuffling points. Trimmed " << temp.size() - points.size() << " points.\n";

}
static inline cv::Point3f makePt(const glm::vec4& v)
{
	return cv::Point3f(v.x, v.y, v.z);
}

static inline vector<cv::Point3f> makeCVCloud(const vector<vec4>& points)
{
	vector<cv::Point3f> result(points.size());
	for (size_t i = 0; i < points.size(); ++i)
		result[i] = makePt(points[i]);
	
	return move(result);
}

static inline pcl::PointXYZI makePclPt(const glm::vec4& pt)
{
	pcl::PointXYZI result(pt.w);
	result.x = pt.x;
	result.y = pt.y;
	result.z = pt.z;
	return std::move(result);
}

static inline vector<pcl::PointXYZI> makePCLCloud(const vector<vec4>& points)
{
	vector<pcl::PointXYZI> result(points.size());
	for (size_t i = 0; i < points.size(); ++i)
		result[i] = makePclPt(points[i]);

	return std::move(result);
}


float ReferencePoints::calculateMeanDistance(const ReferencePoints* reference) const
{
	assert(points.size() == reference->points.size());

	// calculate mean distance/error
	float meanDistance = 0.f;

	for (size_t i = 0; i < points.size(); ++i)
	{
		vec4 d = this->points[i] - reference->points[i];
		meanDistance += sqrtf(dot(d, d));
	}

	meanDistance /= points.size();
	return meanDistance;
}


void ReferencePoints::draw() const
{
	if (points.empty() || normals.empty())
		return;


	glVertexPointer(3, GL_FLOAT, sizeof(vec4), value_ptr(points[0]));
	glColorPointer(3, GL_FLOAT, 0, value_ptr(normals[0]));
	
	glDrawArrays(GL_POINTS, 1, points.size());

	

}

float ReferencePoints::align(const ReferencePoints* reference, mat4& delta)
{
	using namespace pcl;

	vector<PointXYZI> refCloud = makePCLCloud(reference->points);
	vector<PointXYZI> tgtCloud = makePCLCloud(this->points);

	try
	{


	}
	catch (cv::Exception& e)
	{
		std::cout << "[Debug] OpenCV error: " << e.what() << std::endl;
		return 0.f;
	}

	
}

/*
float ReferencePoints::align(const ReferencePoints* reference, mat4& delta)
{
	assert(points.size() == reference->points.size());

	// convert clouds to opencv
	vector<cv::Point3f> refCloud = makeCVCloud(reference->points);
	vector<cv::Point3f> tgtCloud = makeCVCloud(this->points);
	assert(tgtCloud.size() == refCloud.size());

	try
	{

		cv::Mat transformEstimate(3, 4, CV_64F);
		vector<uchar> inliers;

		double ransacThreshold = 1.0;
		double confidence = 0.95;
		cv::estimateAffine3D(tgtCloud, refCloud, transformEstimate, inliers, ransacThreshold, confidence);

		// count inliers:
		unsigned int icount = count_if(inliers.begin(), inliers.end(), [](uchar c) { return c > 0; });
		std::cout << "[Debug] Inliers: " << icount << " (" << (float)icount / tgtCloud.size() << ")\n";

		// transform points
		mat4 deltaTransform = mat4(1.f);
		for (int i = 0; i < 3; ++i)
		{
			for (int j = 0; j < 4; ++j)
			{
				deltaTransform[j][i] = transformEstimate.at<double>(i, j);
				//std::cout << "[Debug] [" << i << "," << j << "]: " << transformEstimate.at<double>(i, j) << std::endl;
			}
		}

		std::cout << "[Debug] Transform (glm): " << deltaTransform << std::endl;
		delta = deltaTransform;

		this->applyTransform(deltaTransform);
	}
	catch (cv::Exception& e)
	{
		std::cout << "[Debug] OpenCV error: " << e.what() << std::endl;
		return 0.f;
	}


}

*/

void ReferencePoints::applyTransform(const mat4& m)
{
	for (size_t i = 0; i < points.size(); ++i)
	{
		float w = points[i].w;
		vec4 pt = m * vec4(vec3(points[i]), 1.f);
		points[i] = pt;
		points[i].w = w;
	}
}

