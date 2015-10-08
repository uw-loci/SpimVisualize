#pragma once

#include <glm/glm.hpp>


#undef near
#undef far

class OrbitCamera
{
public:
	OrbitCamera();

	void setup() const;
	void jitter(float v);

	void getMVP(glm::mat4& mvp) const;
	void getMVPdouble(glm::dmat4& mvp) const;
	void getMatrices(glm::mat4& proj, glm::mat4& view) const;
	void getDoubleMatrices(glm::dmat4& proj, glm::dmat4& view) const;

	void zoom(float delta);
	void rotate(float deltaTheta, float deltaPhi);
	void pan(float deltaX, float deltaY);

	float		fovy, aspect, near, far;
	glm::vec3	target, up;

	float		radius;

	inline glm::vec3 getPosition() const { return target + getOffset(); }
	

private:
	float	theta, phi;

	glm::vec3 getOffset() const;

};
