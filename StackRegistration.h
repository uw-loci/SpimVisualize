#pragma once


#include <vector>
#include <glm/glm.hpp>

class SpimStack;

struct Threshold
{
	unsigned short	min;
	unsigned short	max;
};

class ReferencePoints
{
public:	
	void draw() const;

	
	void trim(const ReferencePoints* smaller);

	float calculateMeanDistance(const ReferencePoints* other) const;

		
	// aligns to
	float align(const ReferencePoints* reference, glm::mat4& deltaTransform);

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
