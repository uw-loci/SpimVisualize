#pragma once

#include <glm/glm.hpp>
#include <boost/utility.hpp>

#include "AABB.h"

class InteractionVolume : boost::noncopyable
{
public:
	glm::mat4		transform;
	bool			enabled;
	
	InteractionVolume();
	virtual ~InteractionVolume() {};

	inline const AABB& getBBox() const { return bbox; };
	virtual AABB getTransformedBBox() const;

	inline void toggle() { enabled = !enabled; }


	virtual void saveTransform(const std::string& filename) const;
	virtual void loadTransform(const std::string& filename);
	virtual void applyTransform(const glm::mat4& t);

	virtual void setRotation(float angle);
	virtual void move(const glm::vec3& delta);
	virtual void rotate(float d);

	
protected:
	AABB			bbox;

};

