
#include "GeometryImage.h"
#include "OrbitCamera.h"
#include "SpimPlane.h"
#include "Shader.h"
#include "Framebuffer.h"
#include "SpimStack.h"
#include "AABB.h"
#include "Layout.h"
#include "SpimRegistrationApp.h"

#include <GL/glew.h>
#include <GL/freeglut.h>

#include <glm/gtx/matrix_operation.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/io.hpp>

#include <fstream>
#include <iostream>
#include <vector>
#include <algorithm>
#include <chrono>

// goddamnit windows! :(
#undef near
#undef far

struct Mouse
{
	glm::ivec2		coordinates;
	int				button[3];
}				mouse;


SpimRegistrationApp*	regoApp = nullptr;


static void display()
{
		
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);	
	
	regoApp->drawScene();
	
	glutSwapBuffers();

}

static void idle()
{

	unsigned int time = glutGet(GLUT_ELAPSED_TIME);
	static unsigned int oldTime = 0;

	float dt = (float)(time - oldTime) / 1000.f;
	oldTime = time;
	
	glutPostRedisplay();
}

static void keyboard(unsigned char key, int x, int y)
{
	if (key == 27)
		exit(0);

	if (key == '/')
		regoApp->toggleSlices();

	if (key == 'g')
		regoApp->toggleGrid();

	if (key == 'b')
		regoApp->toggleBboxes();

	if (key == ',')
		regoApp->decreaseMinThreshold();

	if (key == '.')
		regoApp->increaseMinThreshold();

	
	if (key == '[')
		regoApp->rotateCurrentStack(-1);

	if (key == ']')
		regoApp->rotateCurrentStack(1);
	
	if (key == 'S')
		regoApp->reloadShaders();



	if (key == '1')
		regoApp->toggleSelectStack(0);

	if (key == '2')
		regoApp->toggleSelectStack(1);

	if (key == '3')
		regoApp->toggleSelectStack(2);

	if (key == '4')
		regoApp->toggleSelectStack(3);

	if (key == '5')
		regoApp->toggleSelectStack(4);

	
	/*
	if (key == 't')
	{
		for (int i = 0; i < stacks.size(); ++i)
		{
			char filename[256];
			sprintf(filename, "E:/temp/stack_%02d.registration.txt", i);
			stacks[i]->saveTransform(filename);
		}
	}

	if (key == 'T')
	{
		for (int i = 0; i < stacks.size(); ++i)
		{
			char filename[256];
			sprintf(filename, "E:/temp/stack_%02d.registration.txt", i);
			stacks[i]->loadTransform(filename);
		}

	}
	*/


	if (key == 'v')
		regoApp->toggleCurrentStack();


	if (key == '-')
		regoApp->zoomCamera(0.7f);
	if (key == '=')
		regoApp->zoomCamera(1.4f);
	if (key == 'c')
		regoApp->centerCamera();

	if (key == 'w')
		regoApp->panCamera(glm::vec2(0, 10));
	if (key == 's')
		regoApp->panCamera(glm::vec2(0, -10));
	if (key == 'a')
		regoApp->panCamera(glm::vec2(-10, 0));
	if (key == 'd')
		regoApp->panCamera(glm::vec2(10, 0));

}


static void motion(int x, int y)
{
	glm::ivec2 d = mouse.coordinates - glm::ivec2(x, y);

	float dt = (mouse.coordinates.y - y) * 0.1f;
	float dp = (mouse.coordinates.x - x) * 0.1f;

	//camera.rotate(dt, dp);

	float dx = (x - mouse.coordinates.x) * 2.f;
	float dy = (y - mouse.coordinates.y) * -2.f;

	mouse.coordinates.x = x;
	mouse.coordinates.y = y;


	if (mouse.button[0])
		regoApp->rotateCamera(glm::vec2(dt, dp));
	
	if (mouse.button[1])
		regoApp->panCamera(glm::vec2(dx, dy));


	// drag single stack here ...

	/*
	if (v)
	{

		// drag perspective camera
		if (mouse.button[0] && v->name == Viewport::PERSPECTIVE)
			v->camera->rotate(dt, dp);

		// drag single stack
		if (mouse.button[0] && currentStack > -1 && v->name != Viewport::PERSPECTIVE)
		{
			// create ray
			const glm::vec3 rayOrigin = v->camera->getPosition();
			const glm::vec3 rayDirection = v->camera->getViewDirection();

			stacks[currentStack]->move(v->camera->calculatePlanarMovement(glm::vec2(dx, dy)));
		}


		if (mouse.button[1])
			v->camera->pan(dx * v->getAspect() / 2, dy);
	}
	*/

	int h = glutGet(GLUT_WINDOW_HEIGHT);
	regoApp->updateMouseMotion(glm::ivec2(x, h - y));
}

static void passiveMotion(int x, int y)
{
	int h = glutGet(GLUT_WINDOW_HEIGHT);
	regoApp->updateMouseMotion(glm::ivec2(x, h - y));
}

static void special(int key, int x, int y)
{
	int w = glutGet(GLUT_WINDOW_WIDTH);
	int h = glutGet(GLUT_WINDOW_HEIGHT);
	const glm::ivec2 winRes(w, h);

	if (key == GLUT_KEY_F1)
		regoApp->setPerspectiveLayout(winRes, mouse.coordinates);

	if (key == GLUT_KEY_F2)
		regoApp->setTopviewLayout(winRes, mouse.coordinates);
	
	if (key == GLUT_KEY_F3)
		regoApp->setThreeViewLayout(winRes, mouse.coordinates);
	

	if (key == GLUT_KEY_DOWN)
	{

	}

	if (key == GLUT_KEY_LEFT)
	{


	}


	if (key == GLUT_KEY_RIGHT)
	{

	}

}

static void button(int button, int state, int x, int y)
{
	//std::cout << "Button: " << button << " state: " << state << " x " << x << " y " << y << std::endl;

	mouse.coordinates.x = x;
	mouse.coordinates.y = y;

	mouse.button[button] = (state == GLUT_DOWN);
}

static void reshape(int w, int h)
{
	regoApp->resize(glm::ivec2(w, h));
}

int main(int argc, const char** argv)
{

	if (argc < 2)
	{
		std::cerr << "[Error] No filename given!\n";
		std::cerr << "[Usage] " << argv[0] << " <spimfile>\n";
		return -1;
	}

	
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
	glutReshapeFunc(reshape);
	glutPassiveMotionFunc(passiveMotion);
	glutSpecialFunc(special);
	
	glEnable(GL_DEPTH_TEST);
	glPointSize(2.f);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
			

	regoApp = new SpimRegistrationApp(glm::ivec2(1024,768));

	try
	{
		
		for (int i = 0; i < 1; ++i)
		{
			char filename[256];
			//sprintf(filename, "e:/spim/test/spim_TL00_Angle%d.tif", i);
			//sprintf(filename, "E:/spim/091015 SPIM various size beads/091015 20micron beads/spim_TL01_Angle%d.ome.tiff", i);
			//sprintf(filename, "e:/spim/zebra/spim_TL01_Angle%d.ome.tiff", i);

			sprintf(filename, "e:/spim/zebra_beads/spim_TL01_Angle%d.ome.tiff", i);
			regoApp->addSpimStack(filename);
		}



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

