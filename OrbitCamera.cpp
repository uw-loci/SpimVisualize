#include "OrbitCamera.h"

#include <GL/glew.h>
#include <iostream>
#include <algorithm>

#include <glm/gtx/transform.hpp>
#include <glm/gtx/transform2.hpp>

using namespace glm;
using namespace std;

static inline double deg2rad(double a)
{
	return a * .017453292519943295; // (angle / 180) * Math.PI;
}

OrbitCamera::OrbitCamera() : fovy(60.f), theta(22), phi(10), radius(100)
{
	aspect = 1.3f;
	near = 50.f;
	far = 5000.f;
	target = vec3(0.f);
	up = vec3(0, 1, 0);
}

void OrbitCamera::setup() const
{
	
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(fovy, aspect, near, far);

	vec3 pos = this->getPosition();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	gluLookAt(pos.x, pos.y, pos.z, target.x, target.y, target.z, up.x, up.y, up.z);

}

void OrbitCamera::getMVP(glm::mat4& mvp) const
{
	mat4 mdlv = glm::lookAt(getPosition(), target, up);
	mat4 proj = glm::perspective(fovy, aspect, near, far);

	mvp = proj * mdlv;
}

void OrbitCamera::getMVPdouble(glm::dmat4& mvp) const
{
	dmat4 mdlv = glm::lookAt(dvec3(getPosition()), dvec3(target), dvec3(up));
	dmat4 proj = glm::perspective((double)fovy, (double)aspect, (double)near, (double)far);

	mvp = proj * mdlv;
}

void OrbitCamera::getDoubleMatrices(glm::dmat4& proj, glm::dmat4& view) const
{
	view = glm::lookAt(dvec3(getPosition()), dvec3(target), dvec3(up));
	proj = glm::perspective((double)fovy, (double)aspect, (double)near, (double)far);
}

void OrbitCamera::getMatrices(glm::mat4& proj, glm::mat4& view) const
{
	view = glm::lookAt(vec3(getPosition()), vec3(target), vec3(up));
	proj = glm::perspective(fovy, aspect, near, far);
}



vec3 OrbitCamera::getOffset() const
{
	vec3 off(0.f);

	double t = deg2rad(theta);
	double p = deg2rad(phi);

	off.x = sin(t) * sin(p);
	off.z = sin(t) * cos(p);
	off.y = cos(t);

	return off * radius;
}

void OrbitCamera::zoom(float d) 
{
	radius *= d;

	radius = std::max(near, radius);
	radius = std::min(far, radius);
}

void OrbitCamera::rotate(float deltaTheta, float deltaPhi)
{
	theta += deltaTheta;
	phi += deltaPhi;

	theta = std::max(1.f, theta);
	theta = std::min(179.f, theta);

	//std::cout << "[Debug] theta " << theta << " phi " << phi << std::endl;
}

void OrbitCamera::pan(float deltaX, float deltaY)
{
	vec3 fwd = normalize(-getOffset());
	vec3 right = normalize(cross(fwd, up));
	vec3 up2 = cross(right, fwd);

	up2 *= deltaY;
	right *= deltaX;

	target = target + up2 + right;	
}

void OrbitCamera::jitter(float v)
{
	theta += v;
	phi += v;
}


OrthoCamera::OrthoCamera(const glm::vec3& viewDir, const glm::vec3& up) : viewDirection(normalize(viewDir)), zoomFactor(1000.f)
{
	this->up = up;
	this->target = vec3(0.f);
	this->aspect = 1.3f;
	this->near = -1000.f;
	this->far = 5000.f;

}

void OrthoCamera::setup() const
{
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(-aspect*zoomFactor, aspect*zoomFactor, -zoomFactor, zoomFactor, near, far);

	vec3 pos = this->getPosition();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	gluLookAt(pos.x, pos.y, pos.z, target.x, target.y, target.z, up.x, up.y, up.z);
}

void OrthoCamera::getMatrices(glm::mat4& proj, glm::mat4& view) const
{
	proj = ortho(-aspect*zoomFactor, aspect*zoomFactor, -zoomFactor, zoomFactor, near, far);
	view = lookAt(getPosition(), target, up);
}

void OrthoCamera::getMVP(glm::mat4& mvp) const
{
	mat4 proj = ortho(-aspect*zoomFactor, aspect*zoomFactor, -zoomFactor, zoomFactor, near, far);
	mat4 view = lookAt(getPosition(), target, up);

	mvp = proj * view;
}

void OrthoCamera::pan(float dx, float dy)
{
	vec3 fwd = viewDirection;
	vec3 right = normalize(cross(fwd, up));
	vec3 delta = up * dy + right * dx;

	target += delta;
}

void OrthoCamera::zoom(float z)
{
	zoomFactor *= z;
}
