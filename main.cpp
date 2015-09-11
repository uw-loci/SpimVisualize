
#include "GeometryImage.h"
#include "OrbitCamera.h"
#include "SpimPlane.h"
#include "Shader.h"
#include "Framebuffer.h"
#include "SpimStack.h"
#include "AABB.h"

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



OrbitCamera		camera;

bool drawLines = false;
bool rotate = false;

glm::ivec2		mouse;

Shader*			planeShader = 0;

Shader*			quadShader = 0;

std::vector<SpimPlane*> planes;

Shader*			pointShader = 0;

Shader*			volumeShader = 0;
float			minThreshold = 0.004;

bool			drawBbox = true;

std::vector<SpimStack*>	stacks;
unsigned int currentStack = 0;

static const glm::vec3& getRandomColor(int n)
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

static void reloadShaders()
{
	delete planeShader;
	planeShader = new Shader("shaders/plane.vert", "shaders/plane.frag");


	delete quadShader;
	quadShader = new Shader("shaders/drawQuad.vert", "shaders/drawQuad.frag");

	delete pointShader;
	pointShader = new Shader("shaders/points2.vert", "shaders/points2.frag");


	delete volumeShader;
	volumeShader = new Shader("shaders/volume.vert", "shaders/volume.frag");
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


	// draw spim stack volumes here
	glm::mat4 mvp;
	camera.getMVP(mvp);

	volumeShader->bind();
	volumeShader->setMatrix4("mvpMatrix", mvp);
	volumeShader->setUniform("minThreshold", minThreshold);


	for (size_t i = 0; i < stacks.size(); ++i)
	{
		volumeShader->setMatrix4("transform", stacks[i]->getTransform());
		volumeShader->setUniform("stackId", (int)i);
		volumeShader->setUniform("color", getRandomColor(i));
	
		stacks[i]->drawVolume(volumeShader);
	
	
	}
	volumeShader->disable();


	// draw the points
	if (!stacks[currentStack]->getRegistrationPoints().empty())
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE);

		pointShader->bind();
		pointShader->setMatrix4("mvpMatrix", mvp);
		pointShader->setMatrix4("transform", stacks[currentStack]->getTransform());
		
		glEnableClientState(GL_VERTEX_ARRAY);
		glVertexPointer(3, GL_FLOAT, 0, glm::value_ptr(stacks[currentStack]->getRegistrationPoints()[0]));
		glDrawArrays(GL_POINTS, 0, stacks[currentStack]->getRegistrationPoints().size());
		glDisableClientState(GL_VERTEX_ARRAY);


		pointShader->disable();
		glDisable(GL_BLEND);
	}

	if (drawBbox)
	{
		for (size_t i = 0; i < stacks.size(); ++i)
		{

			glPushMatrix();
			glMultMatrixf(glm::value_ptr(stacks[i]->getTransform()));

			glColor3f(1, 1, 0);
			stacks[i]->getBBox().draw();

			glPopMatrix();
		}
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

	if (key == 'n')
	{
		++currentStack;
		if (currentStack == stacks.size())
			currentStack = 0;
	}

	if (key == 'b')
		drawBbox = !drawBbox;

	if (key == ',')
	{
		minThreshold -= 0.001;
		std::cout << "min thresh: " << minThreshold << std::endl;
	}
	if (key == '.')
	{
		minThreshold += 0.001;
		std::cout << "min thresh: " << minThreshold << std::endl;
	}


	if (key == ' ')
	{
		std::cout << "Creating point cloud from stack " << currentStack << std::endl;
		stacks[currentStack]->calculateRegistrationPoints(minThreshold*std::numeric_limits<unsigned short>::max());
	}

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
		AABB globalBbox;
		globalBbox.reset();

		for (int i = 0; i < 2; ++i)		
		{
			char filename[256];
			//sprintf(filename, "e:/spim/test/spim_TL00_Angle%d.tif", i);
			sprintf(filename, "E:/spim/091015 SPIM various size beads/091015 45 micron beads/spim_TL01_Angle%d.ome.tiff", i);

			SpimStack* stack = new SpimStack(filename);

			float angle = -30 + 30 * i;
			stack->setRotation(angle);

			
			/*
			sprintf(filename, "e:/spim/test/spim_TL00_Angle%d.tif.registration", i);
			stack->loadRegistration(filename);
			*/

			stacks.push_back(stack);
		
			AABB bbox = stack->getBBox();
			globalBbox.extend(bbox);
		}



		camera.setRadius(globalBbox.getSpanLength() * 1.5); // frames[0]->getBBox().getSpanLength() * 1.2);
		camera.target = globalBbox.getCentroid();// frames[0]->getBBox().getCentroid();


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

