
#include "GeometryImage.h"
#include "OrbitCamera.h"
#include "SpimPlane.h"
#include "Shader.h"
#include "Framebuffer.h"

#include <GL/glew.h>
#include <GL/freeglut.h>

#include <glm/gtx/matrix_operation.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <fstream>
#include <iostream>
#include <vector>



OrbitCamera		camera;

bool drawLines = false;
bool rotate = false;

glm::ivec2		mouse;

Shader*			planeShader = 0;

const unsigned int DEPTH_PEEL_LEVELS = 5;
std::vector<Framebuffer*>	renderTargets;
Framebuffer*				pingpong = 0;

unsigned int	currentDisplay = 0;


Shader*			quadShader = 0;

std::vector<SpimPlane*> planes;

static void reloadShaders()
{
	delete planeShader;
	planeShader = new Shader("shaders/plane.vert", "shaders/plane.frag");


	delete quadShader;
	quadShader = new Shader("shaders/drawQuad.vert", "shaders/drawQuad.frag");
}

static void drawScene()
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


	if (planeShader && !planes.empty())
	{

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


		planeShader->bind();
		glm::mat4 mvp(1.f);
		camera.getMVP(mvp);
		planeShader->setMatrix4("mvpMatrix", mvp);


		//TODO: implement depth peeling here


		for (size_t i = 0; i < planes.size(); ++i)
			planes[i]->draw(planeShader);

		planeShader->disable();

		glDisable(GL_BLEND);


	}

}

static void display()
{
	
	camera.setup();
	
	for (size_t i = 0; i < renderTargets.size(); ++i)
	{		
		renderTargets[i]->bind();
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		if (i == 0)
		{
		
			drawScene();
		}


		renderTargets[i]->disable();

	}

	// display quad
	glDisable(GL_DEPTH_TEST);

	quadShader->bind();
	quadShader->setTexture2D("colormap", renderTargets[currentDisplay]->getColorbuffer(), 0);

	glBegin(GL_QUADS);
	glVertex2i(0, 1);
	glVertex2i(0, 0);
	glVertex2i(1, 0);
	glVertex2i(1, 1);
	glEnd();

	quadShader->disable();
	glEnable(GL_DEPTH_TEST);

	glutSwapBuffers();

}

static void idle()
{

	unsigned int time = glutGet(GLUT_ELAPSED_TIME);
	static unsigned int oldTime = 0;

	float dt = (float)(time - oldTime) / 1000.f;
	oldTime = time;

	if (rotate)
		camera.rotate(0.f, 15.f * dt);

	glutPostRedisplay();
}

static void keyboard(unsigned char key, int x, int y)
{
	if (key == 27)
		exit(0);

	if (key == '-')
		camera.zoom(0.7f);
	if (key == '=')
		camera.zoom(1.4f);

	if (key == 'a')
		camera.pan(-1, 0);
	if (key == 'd')
		camera.pan(1, 0);
	if (key == 'w')
		camera.pan(0, 1);
	if (key == 's')
		//camera.pan(0, -1);
		reloadShaders();

	if (key == 'r')
		rotate = !rotate;

	if (key == '1')
		currentDisplay = 0;
	if (key == '2')
		currentDisplay = 1;
	if (key == '3')
		currentDisplay = 2;
	if (key == '4')
		currentDisplay = 3;
	if (key == '5')
		currentDisplay = 4;
	/*
	if (key == '6')
		currentDisplay = 5;
	if (key == '7')
		currentDisplay = 6;
	*/

}


static void motion(int x, int y)
{
	
	float dt = (mouse.y - y) * 0.1f;
	float dp = (mouse.x - x) * 0.1f;


	camera.rotate(dt, dp);

	mouse.x = x;
	mouse.y = y;

}

static void button(int button, int state, int x, int y)
{
	//std::cout << "Button: " << button << " state: " << state << " x " << x << " y " << y << std::endl;

	mouse.x = x;
	mouse.y = y;
}

int main(int argc, const char** argv)
{

	
	glutInit(&argc, const_cast<char**>(argv));
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
	glutInitWindowSize(1024, 768);
	glutCreateWindow("Geometry Image");
	glewInit();

	glutDisplayFunc(display);
	glutIdleFunc(idle);
	glutMouseFunc(button);
	glutKeyboardFunc(keyboard);
	glutMotionFunc(motion);
	
	glEnable(GL_DEPTH_TEST);
	glPointSize(2.f);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	
	try
	{

		for (int k = 0; k < 10; ++k)
		{
			for (int i = 0; i < 5; ++i)
			{
				/*
				char filename[256];
				sprintf(filename, "e:/spim/OpenSPIM_tutorial/tiffs/spim_TL%02d_Angle%d.tif", k, i);

				char filename2[256];
				sprintf(filename2, "e:/spim/OpenSPIM_tutorial/tiffs/registration/spim_TL%02d_Angle%d.tif.registration", k, i);

				SpimPlane* s = nullptr;

				try
				{
					s = new SpimPlane(filename, filename2);
					planes.push_back(s);
				}
				catch (std::runtime_error& e)
				{
					std::cout << "[Error] " << e.what() << std::endl;
					delete s;
				}
				*/




			}
		}

		for (int i = 0; i < DEPTH_PEEL_LEVELS; ++i)
		{
			renderTargets.push_back(new Framebuffer(1024, 1024, GL_RGBA, GL_UNSIGNED_BYTE, 1, GL_LINEAR, true));
		}

		pingpong = new Framebuffer(1024, 1024, GL_RGBA, GL_UNSIGNED_BYTE, 1, GL_LINEAR, true);


		camera.setRadius(500.f); // frames[0]->getBBox().getSpanLength() * 1.2);
		camera.target = glm::vec3(0.f);// frames[0]->getBBox().getCentroid();


		reloadShaders();
	}
	catch (const std::runtime_error& e)
	{
		std::cerr << "[Error] " << e.what() << std::endl;
	}
	catch (const std::string& e)
	{
		std::cerr << "[Error] " << e << std::endl;
	}

	

	glutMainLoop();

	return 0;
}

