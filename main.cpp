
#include "GeometryImage.h"
#include "OrbitCamera.h"
#include "SpimPlane.h"
#include "Shader.h"
#include "Framebuffer.h"
#include "SpimStack.h"

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

unsigned int	currentDisplay = 0;


Shader*			quadShader = 0;

std::vector<SpimPlane*> planes;
std::vector<glm::vec4>	points;

Shader*			pointShader = 0;

static void reloadShaders()
{
	delete planeShader;
	planeShader = new Shader("shaders/plane.vert", "shaders/plane.frag");


	delete quadShader;
	quadShader = new Shader("shaders/drawQuad.vert", "shaders/drawQuad.frag");

	delete pointShader;
	pointShader = new Shader("shaders/points.vert", "shaders/points.frag");
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


	if (!points.empty())
	{
		/*
		glColor3f(1, 1, 1);
		glEnableClientState(GL_VERTEX_ARRAY);
		glVertexPointer(3, GL_FLOAT, sizeof(glm::vec4), glm::value_ptr(points[0]));

		glDrawArrays(GL_POINTS, 0, points.size());

		glDisableClientState(GL_VERTEX_ARRAY);
		*/
		/*
		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);

		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), glm::value_ptr(points[0]));
		glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), &points[0].a);
	
		glDrawArrays(GL_POINTS, 0, points.size());

		glDisableVertexAttribArray(0);
		glDisableVertexAttribArray(1);
		*/

		glm::mat4 mvp;
		camera.getMVP(mvp);

		pointShader->bind();
		pointShader->setMatrix4("mvpMatrix", mvp);

		glColor3f(1, 1, 1);
		glEnableClientState(GL_VERTEX_ARRAY);
		glVertexPointer(4, GL_FLOAT, 0, glm::value_ptr(points[0]));

		glDrawArrays(GL_POINTS, 0, points.size());

		glDisableClientState(GL_VERTEX_ARRAY);

		pointShader->disable();
	}


}

static void display()
{
	
	camera.setup();
	
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);	
	drawScene();
	
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
	
	glEnable(GL_DEPTH_TEST);
	glPointSize(2.f);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	
	try
	{

		SpimStack stack(argv[1]);
		points = stack.getPointcloud(150);
	
		glm::vec3 center(0.f);
		for (size_t i = 0; i < points.size(); ++i)
			center += glm::vec3(points[i]);

		center /= points.size();



		camera.setRadius(500.f); // frames[0]->getBBox().getSpanLength() * 1.2);
		camera.target = center;// frames[0]->getBBox().getCentroid();


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

