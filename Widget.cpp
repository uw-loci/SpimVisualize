#include "Widget.h"
#include "Layout.h"
#include "OrbitCamera.h"
#include "InteractionVolume.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/io.hpp>
#include <glm/gtx/transform2.hpp>

#include <iostream>

#include <GL/glew.h>

using namespace glm;

static inline void setOrtho()
{
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, 1, 0, 1, 0, 1);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

}

static inline void disableZTest()
{
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);

}

static inline void enableZTest()
{
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);

}

IWidget::IWidget() : active(false), volume(nullptr), initialVolumeMatrix(1.f), initialVolumePosition(0.f), initialMouseCoords(0.f), mode(VIEW_RELATIVE)
{

}

IWidget::IWidget(const IWidget& cp) : active(cp.active), volume(cp.volume), initialVolumeMatrix(cp.initialVolumeMatrix), initialVolumePosition(cp.initialVolumePosition), initialMouseCoords(cp.initialMouseCoords), mode(cp.mode)
{

}

IWidget::~IWidget()
{
}

void IWidget::setInteractionVolume(InteractionVolume* v)
{
	active = false;
	volume = v;
}

void IWidget::beginMouseMove(const Viewport* vp, const glm::vec2& m)
{

	if (!volume)
		return;

	active = true;

	initialMouseCoords = m;
	currentMouseCoords = m;

	initialVolumePosition = volume->getTransformedBBox().getCentroid();
	initialVolumeMatrix = volume->getTransform();
	//std::cout << "[Widget] Begin move " << initialMouseCoords << std::endl;
}

void IWidget::updateMouseMove(const Viewport* vp, const glm::vec2& m, float valueStep)
{	
	currentMouseCoords = m;

	if (active && volume)
		this->applyTransform(vp, valueStep);


	//std::cout << "[Widget] Update move " << currentMouseCoords << std::endl;
}

void IWidget::endMouseMove(const Viewport* vp, const glm::vec2& m)
{

	active = false;
	currentMouseCoords = m;


	//std::cout << "[Widget] End move " << initialMouseCoords << std::endl;
}


void IWidget::setMode(const std::string& m)
{
	if (m == "View")
		mode = VIEW_RELATIVE;
	else if (m == "X")
		mode = AXIS_X;
	else if (m == "Y")
		mode = AXIS_Y;
	else if (m == "Z")
		mode = AXIS_Z;
	else
		throw std::runtime_error("Unknown mode \"" + m + "\"!");
		
}

std::string IWidget::getMode() const
{
	switch (mode)
	{
	case VIEW_RELATIVE:
		return "View";
	case AXIS_X:
		return "X";
	case AXIS_Y:
		return "Y";
	case AXIS_Z:
		return "Z";
	default:
		return "Unknown";
	}

}

static void drawPyramid()
{
	using namespace glm;

	const float a = 1.f / 3.f;
	const float b = 2.f / 3.f;

	const static float data[] = {
		0.000000, 0.000000, -1.000000, 0.666667, 0.333333, -0.666667,
		0.000000, 2.000000, 0.000000, 0.666667, 0.333333, -0.666667,
		1.000000, 0.000000, 0.000000, 0.666667, 0.333333, -0.666667,
		0.000000, 0.000000, 1.000000, -0.666667, 0.333333, 0.666667,
		0.000000, 2.000000, 0.000000, -0.666667, 0.333333, 0.666667,
		-1.000000, 0.000000, -0.000000, -0.666667, 0.333333, 0.666667,
		1.000000, 0.000000, 0.000000, 0.666667, 0.333333, 0.666667,
		0.000000, 2.000000, 0.000000, 0.666667, 0.333333, 0.666667,
		0.000000, 0.000000, 1.000000, 0.666667, 0.333333, 0.666667,
		-1.000000, 0.000000, -0.000000, -0.666667, 0.333333, -0.666667,
		0.000000, 2.000000, 0.000000, -0.666667, 0.333333, -0.666667,
		0.000000, 0.000000, -1.000000, -0.666667, 0.333333, -0.666667,
		0.000000, 0.000000, 1.000000, -0.000000, -1.000000, 0.000000,
		-1.000000, 0.000000, -0.000000, -0.000000, -1.000000, 0.000000,
		0.000000, 0.000000, -1.000000, -0.000000, -1.000000, 0.000000,
		1.000000, 0.000000, 0.000000, -0.000000, -1.000000, 0.000000
	};
	
	const static unsigned char indices[] = {
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 12, 14, 15
	};

	glVertexPointer(3, GL_FLOAT, 3 * sizeof(float), reinterpret_cast<const void*>(&data[0]));
	glNormalPointer(GL_FLOAT, 3 * sizeof(float), reinterpret_cast<const void*>(&data[3]));
	
	glDrawElements(GL_TRIANGLES, 18, GL_UNSIGNED_BYTE, indices);
}






void TranslateWidget::applyTransform(const Viewport* vp, float valueStep)
{	
	vec3 delta = vp->camera->calculatePlanarMovement(currentMouseCoords - initialMouseCoords);

	// this approximation works surprisingly well ...
	float scale = length(vp->camera->getPosition() - volume->getTransformedBBox().getCentroid());
	//std::cout << "[Debug] Scale: " << scale << std::endl;;
	

	if (valueStep > 0.f)
		scale = ceil(scale / valueStep) * valueStep;

	delta *= scale;
	
	vec3 d(0.f);

	switch (mode)
	{
	case AXIS_X:
		d.x = delta.x;
		break;
	case AXIS_Y:
		d.y = delta.y; 
		break;
	case AXIS_Z:
		d.z = delta.z;
		break;
	default:
		d = delta; // volume->move(delta);
	}


	//std::cout << "[Widget] Translate: " << d << ", scale: " << scale << std::endl;

	mat4 T = translate(d) * initialVolumeMatrix;
	volume->setTransform(T);
}




void TranslateWidget::draw(const Viewport* vp) const
{
	if (!active)
		return;

	setOrtho();
	disableZTest();

	glBegin(GL_LINES);
	glColor3f(0.3, 0.3, 0.3);
	glVertex2f(initialMouseCoords.x, initialMouseCoords.y); 
	glVertex2f(currentMouseCoords.x, currentMouseCoords.y);
	glEnd();
	

	vp->setup();

	vec2 delta = currentMouseCoords - initialMouseCoords;


	glLineWidth(3.f);
	glBegin(GL_LINES);
	
	
	const vec3 c = volume->getTransformedBBox().getCentroid();

	if (volume)
	{
		
		if (mode == AXIS_X)
		{
			glColor3f(1, 0, 0);
			glVertex3f(initialVolumePosition.x, initialVolumePosition.y, initialVolumePosition.z);
			vec3 d = c - initialVolumePosition;
			d.y = 0.f;
			d.z = 0.f;

			d += initialVolumePosition;
			glVertex3f(d.x, d.y, d.z);


		} 
		else if (mode == AXIS_Y)
		{
			glColor3f(0, 1, 0);
			glVertex3f(initialVolumePosition.x, initialVolumePosition.y, initialVolumePosition.z);
			vec3 d = c - initialVolumePosition;
			d.x = 0.f;
			d.z = 0.f;

			d += initialVolumePosition;
			glVertex3f(d.x, d.y, d.z);
		} 
		else if (mode == AXIS_Z)
		{
			glColor3f(0, 0, 1);
			glVertex3f(initialVolumePosition.x, initialVolumePosition.y, initialVolumePosition.z);
			vec3 d = c - initialVolumePosition;
			d.x = 0.f;
			d.y = 0.f;

			d += initialVolumePosition;
			glVertex3f(d.x, d.y, d.z);
		}
		else
		{
			glColor3f(0, 1, 1);
			glVertex3f(initialVolumePosition.x, initialVolumePosition.y, initialVolumePosition.z);
			glVertex3f(c.x, c.y, c.z);
		}


	}




	glEnd();

	glLineWidth(1);
	enableZTest();
}


void RotateWidget::applyTransform(const Viewport* vp, float valueStep)
{
	vec2 a = initialMouseCoords - vec2(0.5f);
	vec2 b = currentMouseCoords - vec2(0.5f);

	//float delta = acos(dot(a, b) / (length(b)*length(a)));
	float delta = atan2(a.y, a.x) - atan2(b.y, b.x);
	
	//std::cout << "[Widget] Angle: " << delta <<  std::endl;

	float step = radians(valueStep);
	if (step > 0.f)
		delta = ceil(delta / step) * step;

	
	//delta *= -1;

	//float delta = moveScale * (currentMouseCoords - initialMouseCoords).x;

	vec3 axis(0.f);
	switch (mode)
	{
	case AXIS_X:
		axis.x = 1.f;
		break;
	case AXIS_Z:
		axis.z = 1.f;
		break;

	case AXIS_Y:
	default:
		axis.y = 1.f;
		break;

		/*
	default:
		d = vp->camera->getViewDirection(); // volume->move(delta);
	
		*/
	}


	mat4 I = initialVolumeMatrix;
	mat4 T = translate(volume->getBBox().getCentroid());
	
	// simplified from: I*T* rot * I-1*I
	mat4 R = I * T * rotate(delta, axis) * inverse(T);
	volume->setTransform(R);

}

void RotateWidget::draw(const Viewport* vp) const
{
	if (!active)
		return;

	setOrtho();
	disableZTest();


	glBegin(GL_LINES);

	glColor3f(0.3, 0.3, 0.3);
	glVertex2f(0.5, 0.5);
	glVertex2f(initialMouseCoords.x, initialMouseCoords.y);

	glColor3f(0.7, 0.7, 0);
	glVertex2f(0.5, 0.5);
	glVertex2f(currentMouseCoords.x, currentMouseCoords.y);
	
	glColor3f(0.2, 0.2, 0.8);
	glVertex2f(currentMouseCoords.x, currentMouseCoords.y);
	glVertex2f(initialMouseCoords.x, initialMouseCoords.y);
	
	
	glEnd();


	glLineWidth(1);
	enableZTest();
}


void ScaleWidget::applyTransform(const Viewport* vp, float valueStep)
{
	vec2 a = initialMouseCoords - vec2(0.5f);
	vec2 b = currentMouseCoords - vec2(0.5f);

	float delta = length(b) / length(a);

	if (valueStep > 0)
	{
		float step = valueStep / 100;
		delta = ceil(delta / step) * step;
	}

	//std::cout << "[Widget] Scale: " << delta << std::endl;

	mat4 I = initialVolumeMatrix;
	mat4 T = translate(volume->getBBox().getCentroid());
	mat4 S = I * T * scale(vec3(delta)) * inverse(T);

	volume->setTransform(S);


}

void ScaleWidget::draw(const Viewport* vp) const
{
	if (!active)
		return;

	setOrtho();
	disableZTest();

	glBegin(GL_LINES);
	glColor3f(0.7, 0.7, 0);
	


	glColor3f(0.3, 0.3, 0.3);
	glVertex2f(0.5, 0.5);
	glVertex2f(initialMouseCoords.x, initialMouseCoords.y);

	glColor3f(0.7, 0.7, 0);
	glVertex2f(0.5, 0.5);
	glVertex2f(currentMouseCoords.x, currentMouseCoords.y);

	glColor3f(0.2, 0.2, 0.8);
	glVertex2f(currentMouseCoords.x, currentMouseCoords.y);
	glVertex2f(initialMouseCoords.x, initialMouseCoords.y);

	glEnd();










	glLineWidth(1);
	enableZTest();
}
