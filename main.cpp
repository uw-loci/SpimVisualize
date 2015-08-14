
#include "GeometryImage.h"
#include "OrbitCamera.h"

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

static void display()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(60.f, 1.3f, 1.f, 1000.f);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	gluLookAt(50, 50, 50, 0, 0, 0, 0, 1, 0);

	camera.setup();


	glColor3f(0.7, 0.7, 0.7);
	glBegin(GL_LINES);
	for (int i = -100; i <= 100; i += 10)
	{
		glVertex3i(-100, 0, i);
		glVertex3i(100, 0, i);
		glVertex3i(i, 0, -100);
		glVertex3i(i, 0, 100);
	}

	glEnd();






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
		camera.zoom(0.7);
	if (key == '=')
		camera.zoom(1.4);

	if (key == 'a')
		camera.pan(-1, 0);
	if (key == 'd')
		camera.pan(1, 0);
	if (key == 'w')
		camera.pan(0, 1);
	if (key == 's')
		camera.pan(0, -1);

	if (key == 'r')
		rotate = !rotate;

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

	
	try
	{



		camera.setRadius(10.f); // frames[0]->getBBox().getSpanLength() * 1.2);
		camera.target = glm::vec3(0.f);// frames[0]->getBBox().getCentroid();

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

