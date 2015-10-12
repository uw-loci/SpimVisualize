
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
unsigned short	minThreshold = 100;

bool			drawSlices = false;

bool			drawBbox = true;

int				slices = 100;

std::vector<SpimStack*>	stacks;
int currentStack = -1;

float rotationAngle = 0.f;

AABB			globalBBox;

struct Viewport
{
	// window coordinates
	glm::ivec2		position, size;

	// projection and view matrices
	glm::mat4		proj, view;

	glm::vec3		color;
		
	enum ViewportName { ORTHO_X=0, ORTHO_Y, ORTHO_Z, PERSPECTIVE=3} name;
	
	bool			highlighted;
	float			orthoZoomLevel;
	glm::vec2		orthoZenter;

	inline bool isInside(const glm::ivec2& cursor) const
	{
		using namespace glm;
		const ivec2 rc = cursor - position;
		return all(greaterThanEqual(rc, ivec2(0))) && all(lessThanEqual(rc, size));
	}


	void orthoZoom(float z)
	{
		orthoZoomLevel *= z;
		
		const float DIST = globalBBox.getSpanLength()*1.5;
		const float ASPECT = (float)size.x / size.y;

		float D = DIST * orthoZoomLevel;
		proj = glm::ortho(-D*ASPECT, D*ASPECT, -D, D, -DIST, DIST);
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


		glMatrixMode(GL_PROJECTION);
		glLoadMatrixf(glm::value_ptr(proj));
		glMatrixMode(GL_MODELVIEW);
		glLoadMatrixf(glm::value_ptr(view));
	}

	glm::vec3 getCameraPosition() const
	{
		return glm::vec3(glm::inverse(view) * glm::vec4(0.f, 0.f, 0.f, 1.f));
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
	const glm::mat4 mvp = vp.proj * vp.view;

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
				stacks[i]->drawSlices(volumeShader, vp.getCameraPosition());

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

				glColor3f(1, 1, 0);
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
				

		// if this is the perspective window set the camera transform
		if (i == 3)
		{
			camera.setup();
			camera.getMatrices(view[3].proj, view[3].view);
		}

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


static void orthoKeyboard(unsigned char key, int x, int y, int vp)
{
	if (key == '-')
		view[vp].orthoZoom(0.7f);


	if (key == '=')
		view[vp].orthoZoom(1.4f);

}

static void projKeyboard(unsigned char key, int x, int y, int vp)
{
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

	if (key == 's')
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
	
	int vp = getActiveViewport();
	if (vp >= 0 && vp < 3)
		orthoKeyboard(key, x, y, vp);
	else
		projKeyboard(key, x, y, 3);
}


static void motion(int x, int y)
{
	
	float dt = (mouse.y - y) * 0.1f;
	float dp = (mouse.x - x) * 0.1f;


	camera.rotate(dt, dp);

	mouse.x = x;
	mouse.y = y;

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

	mouse.x = x;
	mouse.y = y;
}

static void reshape(int w, int h)
{
	using namespace glm;

	const float ASPECT = (float)w / h;
	const ivec2 VIEWPORT_SIZE = ivec2(w, h) / 2;
	const float DIST = globalBBox.getSpanLength()*1.5;
	
	// setup the 4 views
	view[0].name = Viewport::ORTHO_X;
	view[0].position = ivec2(0, 0);
	view[0].view = lookAt(vec3(DIST-1.f, 0, 0), vec3(0.f), vec3(0, 1, 0));
	
	view[1].name = Viewport::ORTHO_Y;
	view[1].position = ivec2(w / 2, 0);
	view[1].view = lookAt(vec3(0, DIST-1.f, 0), vec3(0.f), vec3(0, 0, 1));
	
	view[2].name = Viewport::ORTHO_Z;
	view[2].position = ivec2(0, h / 2);
	view[2].view = lookAt(vec3(0, 0, DIST-1.f), vec3(0.f), vec3(0, 1, 0));
	
	view[3].name = Viewport::PERSPECTIVE;
	view[3].position = ivec2(w / 2, h / 2);
	
	camera.getMatrices(view[3].proj, view[3].view);


	// adjust the camera's perspective
	camera.aspect = ASPECT;


	for (int i = 0; i < 4; ++i)
	{
		view[i].color = getRandomColor(i);
		view[i].size = VIEWPORT_SIZE;
		view[i].orthoZoom(1.f);
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

	for (int i = 0; i < 4; ++i)
		view[i].orthoZoomLevel = 1.f;

	
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
							
			//stack->setRotation(-30 + i * 30);

			sprintf(filename, "e:/spim/zebra_beads/registration/spim_TL01_Angle%d.ome.tiff.registration", i);
			stack->loadRegistration(filename);
			

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

