#pragma once

#include <glm/glm.hpp>
#include <boost/utility.hpp>

#include "AABB.h"

class InteractionVolume : boost::noncopyable
{
public:
	bool			enabled;
	
	InteractionVolume();
	virtual ~InteractionVolume() {};

	inline const AABB& getBBox() const { return bbox; };
	virtual AABB getTransformedBBox() const;

	inline void toggle() { enabled = !enabled; }


	virtual void saveTransform(const std::string& filename) const;
	virtual void loadTransform(const std::string& filename);
	virtual void applyTransform(const glm::mat4& t);

	/// sets the rotation around its center in degrees
	virtual void setRotation(float angle);
	virtual void move(const glm::vec3& delta);
	/// rotate around its center in d degrees
	virtual void rotate(float d);


	inline const glm::mat4& getTransform() const { return transform; }
	inline const glm::mat4& getInverseTransform() const { return inverseTransform; }
	inline void setTransform(const glm::mat4& t) { transform = t; inverseTransform = glm::inverse(t); }

	inline bool isInsideVolume(const glm::vec4& worldSpacePt) const { return bbox.isInside(glm::vec3(transform * worldSpacePt)); }
	inline bool isInsideVolume(const glm::vec3& worldSpacePt) const { return isInsideVolume(glm::vec4(worldSpacePt, 1.f)); }

	
protected:
	AABB			bbox;
	
private:
	glm::mat4		transform, inverseTransform;

};

