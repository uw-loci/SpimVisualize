
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


struct Mouse
{
	glm::ivec2		coordinates;
	int				button[3];
}				mouse;



Shader*			planeShader = 0;
Shader*			quadShader = 0;

std::vector<SpimPlane*> planes;

Shader*			pointShader = 0;


Shader*			volumeShader = 0;
Shader*			volumeShader2 = 0;
unsigned short	minThreshold = 100;

bool			drawSlices = false;

bool			drawBbox = true;

int				slices = 100;

std::vector<SpimStack*>	stacks;
int currentStack = -1;

AABB			globalBBox;

struct Viewport
{
	// window coordinates
	glm::ivec2		position, size;

	glm::vec3		color;
		
	enum ViewportName { ORTHO_X=0, ORTHO_Y, ORTHO_Z, PERSPECTIVE=3} name;
	
	ICamera*		camera;
	bool			highlighted;

	inline bool isInside(const glm::ivec2& cursor) const
	{
		using namespace glm;
		const ivec2 rc = cursor - position;
		return all(greaterThanEqual(rc, ivec2(0))) && all(lessThanEqual(rc, size));
	}

	inline float getAspect() const
	{
		return (float)size.x / size.y;
	}


	void setup() const
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
};


Viewport		view[4];

static int getActiveViewport()
{
	for (int i = 0; i < 4; ++i)
	{
		if (view[i].highlighted)
			return i;
	}

	return -1;
}

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

static void drawScene(const Viewport& vp)
{
	glm::mat4 mvp;// = vp.proj * vp.view;
	vp.camera->getMVP(mvp);

	// draw a groud grid only in perspective mode
	if (vp.name == Viewport::PERSPECTIVE || vp.name == Viewport::ORTHO_Y)
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

	if (vp.name == Viewport::ORTHO_X)
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

	if (vp.name == Viewport::ORTHO_Z)
	{
		glColor3f(0.3f, 0.3f, 0.3f);
		glBegin(GL_LINES);
		for (int i = -1000; i <= 1000; i += 100)
		{
			glVertex3i(-1000, i, 0);
			glVertex3i(1000, i,0);
			glVertex3i(i, -1000, 0);
			glVertex3i(i, 1000, 0);
		}

		glEnd();
	}


	if (drawSlices)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);

		volumeShader->bind();
		volumeShader->setUniform("minThreshold", (int)minThreshold);
		volumeShader->setUniform("mvpMatrix", mvp);
		
		for (size_t i = 0; i < stacks.size(); ++i)
		{
			if (stacks[i]->isEnabled())
			{
				volumeShader->setUniform("color", getRandomColor(i));
				volumeShader->setMatrix4("transform", stacks[i]->getTransform());


				// calculate view vector
				glm::vec3 view = vp.camera->getViewDirection();
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
			volumeShader2->setUniform(uname, (int)i);

			AABB bbox = stacks[i]->getBBox();
			sprintf(uname, "volume[%d].bboxMax", i);
			volumeShader2->setUniform(uname, bbox.max);
			sprintf(uname, "volume[%d].bboxMin", i);
			volumeShader2->setUniform(uname, bbox.min);

			sprintf(uname, "volume[%d].inverseMVP", i);
			volumeShader2->setMatrix4(uname, glm::inverse(mvp * stacks[i]->getTransform()));
		}


		// draw all slices
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


		glActiveTexture(GL_TEXTURE0);

		volumeShader2->disable();
		glDisable(GL_BLEND);

	}

	// draw the points
	if (currentStack > -1 && !stacks[currentStack]->getRegistrationPoints().empty())
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
	}
}

static void special()
{

}

static void display()
{
		
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);	
	
	for (int i = 0; i < 4; ++i)
	{
		// setup the correct viewport
		view[i].setup();
		drawScene(view[i]);
	}
	
	
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

	if (key == '/')
		drawSlices = !drawSlices;

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
		minThreshold -= 10;
		std::cout << "min thresh: " << minThreshold << std::endl;
	}
	if (key == '.')
	{
		minThreshold += 10;
		std::cout << "min thresh: " << minThreshold << std::endl;
	}

	
	if (key == '[' && currentStack > -1)
	{
		stacks[currentStack]->rotate(-1);
	}

	if (key == ']' && currentStack > -1)
	{
		stacks[currentStack]->rotate(1.f);
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

	if (key == 'S')
		reloadShaders();



	if (key == '1')
	{
		if (currentStack == 0)
			currentStack = -1;
		else
			currentStack = 0;
	}

	if (key == '2' && stacks.size() >= 2)
	{
		if (currentStack == 1)
			currentStack = -1;
		else
			currentStack = 1;
	}

	if (key == '3' && stacks.size() >= 3)
	{
		if (currentStack == 2)
			currentStack = -1;
		else
			currentStack = 2;
	}

	if (key == '4' && stacks.size() >= 4)
	{
		if (currentStack == 3)
			currentStack = -1;
		else
			currentStack = 3;
	}

	if (key == '5' && stacks.size() >= 5)
	{
		if (currentStack == 4)
			currentStack = -1;
		else
			currentStack = 4;
	}

	if (key == 'v' && currentStack > -1)
		stacks[currentStack]->toggle();
	

	// camera controls
	int vp = getActiveViewport();

	if (key == '-')
		view[vp].camera->zoom(0.7f);
	if (key == '=')
		view[vp].camera->zoom(1.4f);

	if (key == 'w')
		view[vp].camera->pan(0.f, 10.f);
	if (key == 's')
		view[vp].camera->pan(0.f, -10.f);
	if (key == 'a')
		view[vp].camera->pan(-10.f, 0.f); 
	if (key == 'd')
		view[vp].camera->pan(10.f, 0.f);


	if (key == 'c')
	{
		//if (view[vp].name == Viewport::PERSPECTIVE)
		{
			if (currentStack == -1)
				view[vp].camera->target = globalBBox.getCentroid();
			else
				view[vp].camera->target = stacks[currentStack]->getBBox().getCentroid();
		}


	}

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

	int vp = getActiveViewport();
	if (vp > -1 && vp < 4)
	{

		// drag perspective camera
		if (mouse.button[0] && view[vp].name == Viewport::PERSPECTIVE)
			view[vp].camera->rotate(dt, dp);

		// drag single stack
		if (mouse.button[0] && currentStack > -1 && view[vp].name != Viewport::PERSPECTIVE)
		{
			stacks[currentStack]->move(view[vp].camera->calculatePlanarMovement(glm::vec2(dx, dy)));
		}


		if (mouse.button[1])
			view[vp].camera->pan(dx * view[vp].getAspect() / 2, dy); 




	}


	int h = glutGet(GLUT_WINDOW_HEIGHT);

	// reset viewpoint highlights
	for (int i = 0; i < 4; ++i)
		view[i].highlighted = view[i].isInside(glm::ivec2(x, h -y));
}

static void passiveMotion(int x, int y)
{

	int h = glutGet(GLUT_WINDOW_HEIGHT);

	// reset viewpoint highlights
	for (int i = 0; i < 4; ++i)
		view[i].highlighted = view[i].isInside(glm::ivec2(x, h-y));

}

static void special(int key, int x, int y)
{
	if (key == GLUT_KEY_UP)
	{

	}

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
	using namespace glm;

	const float ASPECT = (float)w / h;
	const ivec2 VIEWPORT_SIZE = ivec2(w, h) / 2;
	
	// setup the 4 views
	view[0].position = ivec2(0, 0);
	view[1].position = ivec2(w / 2, 0);
	view[2].position = ivec2(0, h / 2);
	view[3].position = ivec2(w / 2, h / 2);
	
	for (int i = 0; i < 4; ++i)
	{
		view[i].color = getRandomColor(i);
		view[i].size = VIEWPORT_SIZE;
		view[i].camera->aspect = ASPECT;
	}
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


	// setup viewports
	view[0].name = Viewport::ORTHO_X;
	view[0].camera = new OrthoCamera(glm::vec3(-1, 0, 0), glm::vec3(0.f, 1.f, 0.f));
	
	view[1].name = Viewport::ORTHO_Y;
	view[1].camera = new OrthoCamera(glm::vec3(0.f, -1, 0), glm::vec3(1.f, 0.f, 0.f));
	
	view[2].name = Viewport::ORTHO_Z;
	view[2].camera = new OrthoCamera(glm::vec3(0,  0.f, -1), glm::vec3(0.f, 1.f, 0.f));
	
	view[3].name = Viewport::PERSPECTIVE;
	view[3].camera = &camera;



	
	try
	{
		globalBBox.reset();


		for (int i = 0; i < 2; ++i)		
		{
			char filename[256];
			//sprintf(filename, "e:/spim/test/spim_TL00_Angle%d.tif", i);
			//sprintf(filename, "E:/spim/091015 SPIM various size beads/091015 20micron beads/spim_TL01_Angle%d.ome.tiff", i);
			//sprintf(filename, "e:/spim/zebra/spim_TL01_Angle%d.ome.tiff", i);

			sprintf(filename, "e:/spim/zebra_beads/spim_TL01_Angle%d.ome.tiff",i);

			SpimStack* stack = new SpimStack(filename);
			
			/*
			stack->setRotation(-30 + i * 30);
						sprintf(filename, "e:/spim/zebra_beads/registration/spim_TL01_Angle%d.ome.tiff.registration", i);
			stack->loadRegistration(filename);
			*/

			stacks.push_back(stack);
		
			AABB bbox = stack->getBBox();
			globalBBox.extend(bbox);
		}



		camera.radius = (globalBBox.getSpanLength() * 1.5); // frames[0]->getBBox().getSpanLength() * 1.2);
		camera.target = globalBBox.getCentroid();// frames[0]->getBBox().getCentroid();


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

