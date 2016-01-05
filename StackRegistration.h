#pragma once


#include <vector>
#include <glm/glm.hpp>

#include <boost/utility.hpp>

class SpimStack;

struct Threshold
{
	double			min;
	double			max;

	double			mean;
	double			stdDeviation;

	inline double getSpread() const { return max - min; }
	inline double getRelativeValue(double v) const { return (v - min) / (max - min); }
};

class ReferencePoints : boost::noncopyable
{
public:	
	void draw() const;

	
	void trim(const ReferencePoints* smaller);

	float calculateMeanDistance(const ReferencePoints* other) const;

		
	// aligns to
	void align(const ReferencePoints* reference, glm::mat4& deltaTransform);

	inline bool empty() const { return points.empty(); }
	inline size_t size() const { return points.size(); }
	inline void clear() { points.clear(); }

	inline void setPoints(const std::vector<glm::vec4>& pts) { points = pts; }


	void applyTransform(const glm::mat4& m);

private:
	friend class SpimStack;

	std::vector<glm::vec4>		points;
	std::vector<glm::vec3>		normals;

};
