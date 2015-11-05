#include "Viewport.h"
#include "OrbitCamera.h"
#include "AABB.h"

#include <GL/glew.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform2.hpp>

using namespace glm;

extern AABB globalBBox;

void Viewport::setup() const
{
	glViewport(position.x, position.y, size.x, size.y);

	// reset any transform
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, size.x, 0, size.y, 0, 1);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();


	// draw a border
	if (highlighted)
	{
		glColor3f(color.r, color.g, color.b);
		glBegin(GL_LINE_LOOP);
		glVertex2i(1, 1);
		glVertex2i(size.x, 1);
		glVertex2i(size.x, size.y);
		glVertex2i(1, size.y);
		glEnd();
	}


	// set correct matrices


	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf(glm::value_ptr(proj));
	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixf(glm::value_ptr(view));
	
}

void Viewport::orthoZoom(float z)
{
	orthoZoomLevel *= z;

	const float DIST = globalBBox.getSpanLength()*1.5;
	const float ASPECT = (float)size.x / size.y;

	float D = DIST * orthoZoomLevel;
	proj = glm::ortho(-D*ASPECT, D*ASPECT, -D, D, -DIST, DIST);
	
}