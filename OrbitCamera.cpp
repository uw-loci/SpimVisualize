#include "OrbitCamera.h"

#include <GL/glew.h>
#include <iostream>

#include <glm/gtx/transform.hpp>
#include <glm/gtx/transform2.hpp>

using namespace glm;
using namespace std;

static inline double deg2rad(double a)
{
	return a * .017453292519943295; // (angle / 180) * Math.PI;
}

OrbitCamera::OrbitCamera() : fovy(60.f), aspect(1.3), near(1.f), far(150.f), target(0, 0, 0), up(0, 1, 0), theta(22), phi(10), radius(20)
{

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

	radius = max(near, radius);
	radius = min(far, radius);
}

void OrbitCamera::rotate(float deltaTheta, float deltaPhi)
{
	theta += deltaTheta;
	phi += deltaPhi;

	theta = max(1.f, theta);
	theta = min(179.f, theta);

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