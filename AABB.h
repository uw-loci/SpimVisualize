#ifndef AABB_INCLUDED
#define AABB_INCLUDED

#include <vector>
#include <glm/glm.hpp>
#include <vector>

struct AABB
{
	glm::vec3	min, max;
	
	enum ClipResult
	{
		OUTSIDE=0,
		INSIDE,
		INTERSECT
	};

	ClipResult isVisible(const glm::mat4& mvp) const;
	void draw() const;
	void drawSolid() const;
	
	void reset();
	void reset(const glm::vec3& v);

	
	void extend(const glm::vec3& pt);
	inline void extend(const AABB& bbox)
	{
		this->extend(bbox.min);
		this->extend(bbox.max);
	}

	std::vector<glm::vec3> getVertices() const;

	/// scales the bbox around its centroid
	void scale(const glm::vec3& s);

	void calculate(const std::vector<glm::vec3>& points);

	/// checks if the ray, defined by the origin o and vector v intersects this box
	bool isIntersectedByRay(const glm::vec3& o, const glm::vec3& v) const;

	bool isInside(const glm::vec3& pt) const;
	bool isInside(const AABB& bbox) const;

	inline glm::vec3 getCentroid() const { return (min + max) * 0.5f; }

	inline glm::vec3 getSpan() const { return max - min; }
	inline float getSpanLength() const { return glm::length(max-min); }

	AABB calculateScreenSpaceBBox(const glm::mat4& mvp) const;

	float getBoundingSphereRadius() const;

};


#endif
