#include "Layout.h"
#include "OrbitCamera.h"
#include "AABB.h"

#include <GL/glew.h>


using namespace std;
using namespace glm;

const float CAMERA_DISTANCE = 4000.f;
const vec3 CAMERA_TARGET(0.f);

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
	assert(camera);
	camera->setup();
	
}

SingleViewLayout::SingleViewLayout(const ivec2& resolution)
{
	viewport.position = ivec2(0);
	viewport.size = resolution;
}

SingleViewLayout::~SingleViewLayout()
{
	delete viewport.camera;
}

Viewport* SingleViewLayout::getActiveViewport()
{
	if (viewport.highlighted)
		return &viewport;
	else
		return nullptr;
}

void SingleViewLayout::updateMouseMove(const glm::ivec2& coords)
{
	if (viewport.isInside(coords))
		viewport.highlighted = true;
	else
		viewport.highlighted = false;
}

void SingleViewLayout::resize(const ivec2& size)
{
	viewport.position = ivec2(0);
	viewport.size = size;
	viewport.camera->aspect = (float)size.x / size.y;
}

TopViewFullLayout::TopViewFullLayout(const ivec2& resolution) : SingleViewLayout(resolution)
{
	viewport.name = Viewport::ORTHO_Y;
	viewport.color = vec3(1.f);
	viewport.camera = new OrthoCamera(vec3(0.f, -1, 0), vec3(1.f, 0.f, 0.f));
	viewport.camera->aspect = (float)resolution.x / resolution.y;
}


PerspectiveFullLayout::PerspectiveFullLayout(const ivec2& resolution) : SingleViewLayout(resolution)
{
	viewport.camera = new OrbitCamera;
	viewport.color = vec3(1.f);
	viewport.name = Viewport::PERSPECTIVE;

	OrbitCamera* cam = dynamic_cast<OrbitCamera*>(viewport.camera);
	cam->far = CAMERA_DISTANCE * 4.0;
	cam->near = cam->far / 100.0;
	cam->aspect = (float)resolution.x / resolution.y;

	cam->radius = (CAMERA_DISTANCE * 1.5);
	cam->target = CAMERA_TARGET;

}

FourViewLayout::FourViewLayout(const ivec2& size)
{
	views[0].name = Viewport::ORTHO_X;
	views[0].camera = new OrthoCamera(glm::vec3(-1, 0, 0), glm::vec3(0.f, 1.f, 0.f));
	views[0].color = vec3(1, 0, 0);

	views[1].name = Viewport::ORTHO_Y;
	views[1].camera = new OrthoCamera(glm::vec3(0.f, -1, 0), glm::vec3(1.f, 0.f, 0.f));
	views[1].color = vec3(0, 1, 0);

	views[2].name = Viewport::ORTHO_Z;
	views[2].camera = new OrthoCamera(glm::vec3(0, 0.f, -1), glm::vec3(0.f, 1.f, 0.f));
	views[2].color = vec3(0, 0, 1);

	views[3].name = Viewport::PERSPECTIVE;
	views[3].camera = new OrbitCamera;
	views[3].color = vec3(1);

	OrbitCamera* cam = dynamic_cast<OrbitCamera*>(views[3].camera);
	cam->far = CAMERA_DISTANCE * 4.0;
	cam->near = cam->far / 100.0;
	cam->radius = (CAMERA_DISTANCE * 1.5);
	cam->target = CAMERA_TARGET;

	FourViewLayout::resize(size);

}

void FourViewLayout::resize(const ivec2& size)
{
	using namespace glm;

	const float ASPECT = (float)size.x / size.y;
	const ivec2 VIEWPORT_SIZE = size / 2;

	// setup the 4 views
	views[0].position = ivec2(0, 0);
	views[1].position = ivec2(size.x / 2, 0);
	views[2].position = ivec2(0, size.y / 2);
	views[3].position = ivec2(size.x / 2, size.y / 2);



	for (int i = 0; i < 4; ++i)
	{
		views[i].size = VIEWPORT_SIZE;
		views[i].camera->aspect = ASPECT;
	}
}

void FourViewLayout::updateMouseMove(const ivec2& m)
{
	for (int i = 0; i < 4; ++i)
		if (views[i].isInside(m))
			views[i].highlighted = true;
		else
			views[i].highlighted = false;

}

Viewport* FourViewLayout::getActiveViewport()
{
	for (int i = 0; i < 4; ++i)
		if (views[i].highlighted)
			return &views[i];

	return nullptr;
}