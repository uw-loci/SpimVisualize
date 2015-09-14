
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
Shader*			volumeShader2 = 0;
float			minThreshold = 0.004;

bool			drawSlices = false;

bool			drawBbox = true;

int				slices = 100;

std::vector<SpimStack*>	stacks;
unsigned int currentStack = 0;
float rotationAngle = 0.f;

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

	delete volumeShader2;
	volumeShader2 = new Shader("shaders/volume2.vert", "shaders/volume2.frag");
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


	// draw spim stack volumes here
	glm::mat4 proj, view;
	camera.getMatrices(proj, view);


	glm::mat4 mvp(1.f);
	camera.getMVP(mvp);
	
	if (drawSlices)
	{
		volumeShader->bind();
		volumeShader->setUniform("minThreshold", minThreshold);
		volumeShader->setUniform("mvpMatrix", mvp);


		for (size_t i = 0; i < stacks.size(); ++i)
		{
			volumeShader->setUniform("color", getRandomColor(i));
			volumeShader->setMatrix4("transform", stacks[i]->getTransform());
			stacks[i]->drawSlices(volumeShader);
		}

		volumeShader->disable();
	}
	else
	{

		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE);

		volumeShader2->bind();
		volumeShader2->setUniform("minThreshold", minThreshold);
		volumeShader2->setUniform("sliceCount", slices);

		const glm::vec3 camPos = camera.getPosition();
		const glm::vec3 viewDir = glm::normalize(camera.target - camPos);

		// the smallest and largest projected bounding box vertices -- used to calculate the extend of planes
		// to draw
		glm::vec3 minPVal(std::numeric_limits<float>::max()), maxPVal(std::numeric_limits<float>::lowest());

		for (size_t i = 0; i < stacks.size(); ++i)
		{

			volumeShader2->setMatrix4("inverseMVP", glm::inverse(mvp * stacks[i]->getTransform()));
			
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_3D, stacks[i]->getTexture());
			volumeShader2->setUniform("colormap", 0);

			AABB bbox = stacks[i]->getBBox();
			volumeShader2->setUniform("bboxMax", bbox.max);
			volumeShader2->setUniform("bboxMin", bbox.min);


			// draw screen filling quads
			// find max/min distances of bbox cube from camera
			std::vector<glm::vec3> boxVerts = bbox.getVertices();

			// calculate max/min distance
			float maxDist = 0.f, minDist = std::numeric_limits<float>::max();
			for (size_t k = 0; k < boxVerts.size(); ++k)
			{
				glm::vec4 p = mvp * stacks[i]->getTransform() * glm::vec4(boxVerts[k], 1.f);
				p /= p.w;

				minPVal = glm::min(minPVal, glm::vec3(p));
				maxPVal = glm::max(maxPVal, glm::vec3(p));

			}

			//std::cout << "Dist: " << minDist << " - " << maxDist << std::endl;

			maxPVal = glm::min(maxPVal, glm::vec3(1.f));
			minPVal = glm::max(minPVal, glm::vec3(-1.f));

			//std::cout << "Max: " << maxPVal << " min: " << minPVal << std::endl;

			glBegin(GL_QUADS);
			for (int z = 0; z < slices; ++z)
			{
				// render back-to-front
				float zf = glm::mix(maxPVal.z, minPVal.z, (float)z / slices);

				glVertex3f(minPVal.x, maxPVal.y, zf);
				glVertex3f(minPVal.x, minPVal.y, zf);
				glVertex3f(maxPVal.x, minPVal.y, zf);
				glVertex3f(maxPVal.x, maxPVal.y, zf);
			}
			glEnd();


			//stacks[i]->drawSlices(volumeShader);


		}
		volumeShader2->disable();
		glDisable(GL_BLEND);

	}

	// draw the points
	if (!stacks[currentStack]->getRegistrationPoints().empty())
	{
		pointShader->bind();
		pointShader->setMatrix4("mvpMatrix", mvp);
		pointShader->setMatrix4("transform", stacks[currentStack]->getTransform());
		
		glEnableClientState(GL_VERTEX_ARRAY);
		glVertexPointer(3, GL_FLOAT, 0, glm::value_ptr(stacks[currentStack]->getRegistrationPoints()[0]));
		glDrawArrays(GL_POINTS, 0, stacks[currentStack]->getRegistrationPoints().size());
		glDisableClientState(GL_VERTEX_ARRAY);


		pointShader->disable();
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


	if (key == '/')
		drawSlices = !drawSlices;

	if (key == 'r')
		rotate = !rotate;

	if (key == 'n')
	{
		++currentStack;
		if (currentStack == stacks.size())
			currentStack = 0;

		std::cout << "Current stack: " << currentStack << std::endl;
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

	if (key == '[')
	{
		--rotationAngle;
		std::cout << "Current rot angle: " << rotationAngle << std::endl;

		stacks[currentStack]->setRotation(rotationAngle);
	}

	if (key == ']')
	{
		++rotationAngle;
		std::cout << "Current rot angle: " << rotationAngle << std::endl;


		stacks[currentStack]->setRotation(rotationAngle);
	}


	if (key == ' ')
	{
		std::cout << "Creating point cloud from stack " << currentStack << std::endl;
		stacks[currentStack]->calculateRegistrationPoints(minThreshold*std::numeric_limits<unsigned short>::max());
	}

	if (key == '<' && slices > 1)
	{
		slices /= 2;;
		if (slices == 0)
			slices = 1;
		std::cout << "slices: " << slices << std::endl;
	}

	if (key == '>')
	{
		slices *= 2;
		std::cout << "slices: " << slices << std::endl;
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

		for (int i = 0; i < 1; ++i)		
		{
			char filename[256];
			//sprintf(filename, "e:/spim/test/spim_TL00_Angle%d.tif", i);
			sprintf(filename, "E:/spim/091015 SPIM various size beads/091015 20micron beads/spim_TL01_Angle%d.ome.tiff", i);

			SpimStack* stack = new SpimStack(filename);
									
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

