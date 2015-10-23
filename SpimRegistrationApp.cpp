#include "SpimRegistrationApp.h"

#include "Layout.h"
#include "Shader.h"
#include "SpimStack.h"
#include "OrbitCamera.h"

#include <iostream>
#include <GL/glew.h>
#include <glm/gtc/type_ptr.hpp>

SpimRegistrationApp::SpimRegistrationApp(const glm::ivec2& res) : pointShader(0), sliceShader(0), volumeShader(0), layout(0), 
drawGrid(true), drawBboxes(false), drawSlices(false), currentStack(-1), sliceCount(100), beadThreshold(100)
{
	globalBBox.reset();
	layout = new PerspectiveFullLayout(res);

	reloadShaders();
}

SpimRegistrationApp::~SpimRegistrationApp()
{
	delete pointShader;
	delete sliceShader;
	delete volumeShader;
	
	for (size_t i = 0; i < stacks.size(); ++i)
		delete stacks[i];

	delete layout;
}

void SpimRegistrationApp::reloadShaders()
{
	delete pointShader;
	pointShader = new Shader("shaders/points2.vert", "shaders/points2.frag");

	delete sliceShader;
	sliceShader = new Shader("shaders/volume.vert", "shaders/volume.frag");

	delete volumeShader;
	volumeShader = new Shader("shaders/volume2.vert", "shaders/volume2.frag");
}


void SpimRegistrationApp::drawScene()
{
	for (size_t i = 0; i < layout->getViewCount(); ++i)
	{
		const Viewport* vp = layout->getView(i);
		vp->setup();

		drawScene(vp);
	}

}

void SpimRegistrationApp::addSpimStack(const std::string& filename)
{
	SpimStack* stack = new SpimStack(filename);
	stack->subsample();
	stack->subsample();
	stack->subsample();


	stacks.push_back(stack);
	AABB bbox = stack->getBBox();
	
	if (stacks.size() == 1)
		globalBBox = bbox;
	else
		globalBBox.extend(bbox);
}

void SpimRegistrationApp::drawScene(const Viewport* vp)
{
	glm::mat4 mvp;// = vp.proj * vp.view;
	vp->camera->getMVP(mvp);


	if (drawGrid)
	{
		if (vp->name == Viewport::PERSPECTIVE || vp->name == Viewport::ORTHO_Y)
		{
			glColor3f(0.3f, 0.3f, 0.3f);
			glBegin(GL_LINES);
			for (int i = -1000; i <= 1000; i += 100)
			{
				glVertex3i(-1000, 0, i);
				glVertex3i(1000, 0, i);
				glVertex3i(i, 0, -1000);
				glVertex3i(i, 0, 1000);
			}

			glEnd();
		}

		if (vp->name == Viewport::ORTHO_X)
		{
			glColor3f(0.3f, 0.3f, 0.3f);
			glBegin(GL_LINES);
			for (int i = -1000; i <= 1000; i += 100)
			{
				glVertex3i(0, -1000, i);
				glVertex3i(0, 1000, i);
				glVertex3i(0, i, -1000);
				glVertex3i(0, i, 1000);
			}

			glEnd();
		}

		if (vp->name == Viewport::ORTHO_Z)
		{
			glColor3f(0.3f, 0.3f, 0.3f);
			glBegin(GL_LINES);
			for (int i = -1000; i <= 1000; i += 100)
			{
				glVertex3i(-1000, i, 0);
				glVertex3i(1000, i, 0);
				glVertex3i(i, -1000, 0);
				glVertex3i(i, 1000, 0);
			}

			glEnd();
		}
	}

	if (drawSlices)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);

		sliceShader->bind();
		sliceShader->setUniform("beadThreshold", (int)beadThreshold);
		sliceShader->setUniform("mvpMatrix", mvp);

		for (size_t i = 0; i < stacks.size(); ++i)
		{
			if (stacks[i]->isEnabled())
			{
				sliceShader->setUniform("color", getRandomColor(i));
				sliceShader->setMatrix4("transform", stacks[i]->getTransform());


				// calculate view vector
				glm::vec3 view = vp->camera->getViewDirection();
				stacks[i]->drawSlices(volumeShader, view);

			}


		}

		volumeShader->disable();

		glDisable(GL_BLEND);
		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_TRUE);
	}
	else
	{

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		volumeShader->bind();
		volumeShader->setUniform("beadThreshold", beadThreshold);
		volumeShader->setUniform("sliceCount", (float)sliceCount);


		const glm::vec3 camPos = vp->camera->getPosition();
		const glm::vec3 viewDir = glm::normalize(vp->camera->target - camPos);

		// the smallest and largest projected bounding box vertices -- used to calculate the extend of planes
		// to draw
		glm::vec3 minPVal(std::numeric_limits<float>::max()), maxPVal(std::numeric_limits<float>::lowest());

		for (size_t i = 0; i < stacks.size(); ++i)
		{
			if (!stacks[i]->isEnabled())
				continue;


			// draw screen filling quads
			// find max/min distances of bbox cube from camera
			std::vector<glm::vec3> boxVerts = stacks[i]->getBBox().getVertices();

			// calculate max/min distance
			float maxDist = 0.f, minDist = std::numeric_limits<float>::max();
			for (size_t k = 0; k < boxVerts.size(); ++k)
			{
				glm::vec4 p = mvp * stacks[i]->getTransform() * glm::vec4(boxVerts[k], 1.f);
				p /= p.w;

				minPVal = glm::min(minPVal, glm::vec3(p));
				maxPVal = glm::max(maxPVal, glm::vec3(p));

			}

		}

		maxPVal = glm::min(maxPVal, glm::vec3(1.f));
		minPVal = glm::max(minPVal, glm::vec3(-1.f));


		for (size_t i = 0; i < stacks.size(); ++i)
		{
			glActiveTexture(GL_TEXTURE0 + i);
			glBindTexture(GL_TEXTURE_3D, stacks[i]->getTexture());

			char uname[256];
			sprintf(uname, "volume[%d].texture", i);
			volumeShader->setUniform(uname, (int)i);

			AABB bbox = stacks[i]->getBBox();
			sprintf(uname, "volume[%d].bboxMax", i);
			volumeShader->setUniform(uname, bbox.max);
			sprintf(uname, "volume[%d].bboxMin", i);
			volumeShader->setUniform(uname, bbox.min);

			sprintf(uname, "volume[%d].enabled", i);
			volumeShader->setUniform(uname, stacks[i]->isEnabled());

			sprintf(uname, "volume[%d].inverseMVP", i);
			volumeShader->setMatrix4(uname, glm::inverse(mvp * stacks[i]->getTransform()));
		}


		// draw all slices
		glBegin(GL_QUADS);
		for (int z = 0; z < sliceCount; ++z)
		{
			// render back-to-front
			float zf = glm::mix(maxPVal.z, minPVal.z, (float)z / sliceCount);

			glVertex3f(minPVal.x, maxPVal.y, zf);
			glVertex3f(minPVal.x, minPVal.y, zf);
			glVertex3f(maxPVal.x, minPVal.y, zf);
			glVertex3f(maxPVal.x, maxPVal.y, zf);
		}
		glEnd();


		glActiveTexture(GL_TEXTURE0);

		volumeShader->disable();
		glDisable(GL_BLEND);

	}

	if (drawBboxes)
	{
		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);

		for (size_t i = 0; i < stacks.size(); ++i)
		{
			if (stacks[i]->isEnabled())
			{
				glPushMatrix();
				glMultMatrixf(glm::value_ptr(stacks[i]->getTransform()));


				if (i == currentStack)
					glColor3f(1, 1, 0);
				else
					glColor3f(0.6, 0.6, 0.6);
				stacks[i]->getBBox().draw();

				glPopMatrix();
			}
		}
		
		glDepthMask(GL_TRUE);
		glEnable(GL_DEPTH_TEST);
	}

	
	if (!TEST_points.empty())
	{
		std::cout << "[Debug] Drawing " << TEST_points.size() << " points.\n";

		// draw points
		glEnableClientState(GL_VERTEX_ARRAY);
		glVertexPointer(3, GL_FLOAT, sizeof(glm::vec4), glm::value_ptr(TEST_points[0]));

		glPointSize(2.f);
		glColor3f(1.f, 0.f, 0.f);
		glDrawArrays(GL_POINTS, 1, TEST_points.size());

		glDisableClientState(GL_VERTEX_ARRAY);

	}
	
}

glm::vec3 SpimRegistrationApp::getRandomColor(int n)
{
	static std::vector<glm::vec3> pool;
	if (pool.empty() || n >= pool.size())
	{
		pool.push_back(glm::vec3(1, 0, 0));
		pool.push_back(glm::vec3(1, 0.6, 0));
		pool.push_back(glm::vec3(1, 1, 0));
		pool.push_back(glm::vec3(0, 1, 0));
		pool.push_back(glm::vec3(0, 1, 1));
		pool.push_back(glm::vec3(0, 0, 1));
		pool.push_back(glm::vec3(1, 0, 1));
		pool.push_back(glm::vec3(1, 1, 1));


		for (int i = 0; i < (n + 1) * 2; ++i)
		{
			float r = (float)rand() / RAND_MAX;
			float g = (float)rand() / RAND_MAX;
			float b = 1.f - (r + g);
			pool.push_back(glm::vec3(r, g, b));
		}
	}

	return pool[n];
}

void SpimRegistrationApp::resize(const glm::ivec2& newSize)
{
	layout->resize(newSize);
}


void SpimRegistrationApp::setPerspectiveLayout(const glm::ivec2& res, const glm::ivec2& mouseCoords)
{
	std::cout << "[Layout] Creating fullscreen perspective layout ... \n";
	delete layout;
	layout = new PerspectiveFullLayout(res);
	layout->updateMouseMove(mouseCoords);
}

void SpimRegistrationApp::setTopviewLayout(const glm::ivec2& res, const glm::ivec2& mouseCoords)
{
	std::cout << "[Layout] Creating fullscreen top-view layout ... \n";
	delete layout;
	layout = new TopViewFullLayout(res);;
	layout->updateMouseMove(mouseCoords);
}


void SpimRegistrationApp::setThreeViewLayout(const glm::ivec2& res, const glm::ivec2& mouseCoords)
{
	std::cout << "[Layout] Creating four-view layout ... \n";
	delete layout;
	layout = new FourViewLayout(res);
	layout->updateMouseMove(mouseCoords);
}


void SpimRegistrationApp::centerCamera()
{
	Viewport* vp = layout->getActiveViewport();

	if (vp)
	{
		if (currentStack == -1)
			vp->camera->target = globalBBox.getCentroid();
		else
			vp->camera->target = stacks[currentStack]->getBBox().getCentroid();
	}
}


void SpimRegistrationApp::zoomCamera(float z)
{
	Viewport* vp = layout->getActiveViewport();

	if (vp)
		vp->camera->zoom(z);
}

void SpimRegistrationApp::panCamera(const glm::vec2& delta)
{

	Viewport* vp = layout->getActiveViewport();
	if (vp)
		vp->camera->pan(delta.x * vp->getAspect(), delta.y);
}

void SpimRegistrationApp::rotateCamera(const glm::vec2& delta)
{
	Viewport* vp = layout->getActiveViewport();
	if (vp && vp->name == Viewport::PERSPECTIVE)
		vp->camera->rotate(delta.x, delta.y);
}


void SpimRegistrationApp::updateMouseMotion(const glm::ivec2& cursor)
{
	layout->updateMouseMove(cursor);
}

void SpimRegistrationApp::toggleSelectStack(int n)
{
	if (n >= stacks.size())
		currentStack = -1;

	if (currentStack == n)
		currentStack = -1;
	else
		currentStack = n;
}

void SpimRegistrationApp::toggleStack(int n)
{
	if (n >= stacks.size() || n < 0)
		return;

	stacks[n]->toggle();
}

void SpimRegistrationApp::rotateCurrentStack(float rotY)
{
	if (!currentStackValid())
		return;

	stacks[currentStack]->rotate(rotY);
}


void SpimRegistrationApp::moveStack(const glm::vec2& delta)
{
	if (!currentStackValid())
		return;

	Viewport* vp = layout->getActiveViewport();
	if (vp && vp->name != Viewport::PERSPECTIVE)
	{
		stacks[currentStack]->move(vp->camera->calculatePlanarMovement(delta));
	}
}


void SpimRegistrationApp::TEST_alignStacks()
{
	using namespace glm;

	if (stacks.size() < 2)
		return;

	TEST_points.clear();

	mat4 delta(1.f);
	stacks[1]->alignSingleStep(stacks[0], delta, TEST_points);

}