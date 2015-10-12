#pragma once

#include <glm/glm.hpp>


#undef near
#undef far

class ICamera
{
public:
	virtual ~ICamera() {};

	virtual void setup() const = 0;

	virtual void getMVP(glm::mat4& mvp) const = 0;
	virtual void getMatrices(glm::mat4& proj, glm::mat4& view) const = 0;

	virtual void zoom(float d) = 0;
	virtual void rotate(float dt, float dp) = 0;
	virtual void pan(float dx, float dy) = 0;

	virtual glm::vec3 getPosition() const = 0;


	float		aspect, near, far;
	glm::vec3	target, up;



};


class OrbitCamera : public ICamera
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

	float		fovy;
	float		radius;

	inline glm::vec3 getPosition() const { return target + getOffset(); }
	

private:
	float	theta, phi;

	glm::vec3 getOffset() const;

};

class OrthoCamera: public ICamera
{
public:
	virtual void setup() const;

	virtual void getMVP(glm::mat4& mvp) const;
	virtual void getMatrices(glm::mat4& proj, glm::mat4& view) const;

	virtual void zoom(float d);
	virtual void rotate(float dt, float dp);
	virtual void pan(float dx, float dy);

	virtual glm::vec3 getPosition() const;


private:
	float zoomFactor;

};