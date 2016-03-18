#include "OrbitCamera.h"
#include "AABB.h"

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

OrbitCamera::OrbitCamera() : fovy(60.f), radius(100), theta(22), phi(10)
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
	mat4 proj = glm::perspective((float)deg2rad(fovy), aspect, near, far);

	mvp = proj * mdlv;
}

void OrbitCamera::getMVPdouble(glm::dmat4& mvp) const
{
	dmat4 mdlv = glm::lookAt(dvec3(getPosition()), dvec3(target), dvec3(up));
	dmat4 proj = glm::perspective(deg2rad((double)fovy), (double)aspect, (double)near, (double)far);

	mvp = proj * mdlv;
}

void OrbitCamera::getDoubleMatrices(glm::dmat4& proj, glm::dmat4& view) const
{
	view = glm::lookAt(dvec3(getPosition()), dvec3(target), dvec3(up));
	proj = glm::perspective(deg2rad((double)fovy), (double)aspect, (double)near, (double)far);
}

void OrbitCamera::getMatrices(glm::mat4& proj, glm::mat4& view) const
{
	view = glm::lookAt(vec3(getPosition()), vec3(target), vec3(up));
	proj = glm::perspective((float)deg2rad(fovy), aspect, near, far);
}



vec3 OrbitCamera::getOffset() const
{
	vec3 off(0.f);

	double t = deg2rad(theta);
	double p = deg2rad(phi);

	off.x = (float)(sin(t) * sin(p));
	off.z = (float)(sin(t) * cos(p));
	off.y = (float)cos(t);

	return off * radius;
}

void OrbitCamera::zoom(float d) 
{
	radius *= d;

	radius = std::max(near, radius);
	radius = std::min(far, radius);

	std::cout << "[Debug] Camera: new radius: " << radius << std::endl;
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


glm::vec3 OrbitCamera::calculatePlanarMovement(const glm::vec2& delta) const
{
	vec3 fwd = normalize(-getOffset());
	vec3 right = normalize(cross(fwd, up));

	return right * delta.x + up * delta.y;
}


void OrbitCamera::jitter(float v)
{
	theta += v;
	phi += v;
}


void OrbitCamera::maximizeView(const AABB& bbox)
{
	target = bbox.getCentroid();

	float boundSphere = bbox.getBoundingSphereRadius();
	

	
	// calculate the best distance to the centroid based on the fovy and the sphere radius
	//radius = boundSphere * tan(glm::radians(fovy) * 0.5f);
	
	radius = boundSphere;

	std::cout << "[Debug] Camera: calculated radius: " << radius << std::endl;

	
}


OrthoCamera::OrthoCamera(const glm::vec3& viewDir, const glm::vec3& up, float z) : zoomFactor(z), viewDirection(normalize(viewDir))
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


glm::vec3 OrthoCamera::calculatePlanarMovement(const glm::vec2& delta) const
{
	vec3 fwd = viewDirection;
	vec3 right = normalize(cross(fwd, up));

	return right * delta.x + up * delta.y;
}


void OrthoCamera::maximizeView(const AABB& bbox)
{
	target = bbox.getCentroid();

	vec3 maxVal = max(abs(bbox.min), bbox.max);
	zoomFactor = glm::max(maxVal.x, glm::max(maxVal.y, maxVal.z));
}

UnitCamera::UnitCamera()
{
	this->target = vec3(0.f);
	this->up = vec3(0, 1, 0);
	this->near = 0.f;
	this->far = 1.f;
}

void UnitCamera::setup() const
{
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, 1, 0, 1, 0.f, 1.f);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	//gluLookAt(pos.x, pos.y, pos.z, target.x, target.y, target.z, up.x, up.y, up.z);
	glTranslatef(-target.x, -target.y, -target.z);
}

void UnitCamera::getMatrices(glm::mat4& proj, glm::mat4& view) const
{
	proj = ortho(0.f, 1.f, 0.f, 1.f, near, far);
	view = translate(-target);
}

void UnitCamera::getMVP(glm::mat4& mvp) const
{
	mat4 proj = ortho(0.f, 1.f, 0.f, 1.f, 0.f, 1.f);
	mat4 view = translate(-target);

	mvp = proj * view;
}

void UnitCamera::maximizeView(const AABB& bbox)
{
	/*
	target = bbox.getCentroid();

	vec3 maxVal = max(abs(bbox.min), bbox.max);
	//zoomFactor = glm::max(maxVal.x, glm::max(maxVal.y, maxVal.z));
	*/

	throw std::runtime_error("UnitCamera::maximizeView not implemented!");
}