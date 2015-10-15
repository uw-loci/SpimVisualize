
#include "GeometryImage.h"
#include "OrbitCamera.h"
#include "SpimPlane.h"
#include "Shader.h"
#include "Framebuffer.h"
#include "SpimStack.h"
#include "AABB.h"
#include "Layout.h"


#include <ICP.h>

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



Shader*			planeShader = 0;
Shader*			quadShader = 0;

std::vector<SpimPlane*> planes;

Shader*			pointShader = 0;


Shader*			volumeShader = 0;
Shader*			volumeShader2 = 0;
unsigned short	minThreshold = 100;

bool			drawSlices = false;
bool			drawGrid = true;
bool			drawBbox = true;
bool			drawPoints = false;

int				slices = 100;

std::vector<SpimStack*>	stacks;
int currentStack = -1;

AABB			globalBBox;

typedef Eigen::Matrix<double, 3, Eigen::Dynamic> Vertices;
Vertices source, target;

ILayout*			layout = nullptr;


static void alignStacks(SpimStack* a, SpimStack* b)
{
	using namespace glm;
	using namespace std;

	const unsigned short THRESHOLD = 210;

	// 1) extract registration points
	vector<vec4> pa = a->extractRegistrationPoints(THRESHOLD);
	vector<vec4> pb = b->extractRegistrationPoints(THRESHOLD);

	// 2) reset color to homog. coord to enable transformation
	for_each(pa.begin(), pa.end(), [](vec4& v) { v.w = 1.f; });
	for_each(pb.begin(), pb.end(), [](vec4& v) { v.w = 1.f; });

	//std::cout << "[Fit] Extracted " << pa.size() << "/" << pb.size() << " points.\n";

	// 3) transform points to world space
	const mat4& Ma = a->getTransform();
	const mat4& Mb = b->getTransform();
	for_each(pa.begin(), pa.end(), [&Ma](vec4& v) { v = Ma * v; });
	for_each(pb.begin(), pb.end(), [&Mb](vec4& v) { v = Mb * v; });


	/*
	// 4) kill points that are not inside the other's bounding box (after transformation)
	OBB bboxA;
	bboxA.transform = a->getTransform();
	bboxA.bbox = a->getBBox();

	OBB bboxB;
	bboxB.transform = b->getTransform();
	bboxB.bbox = b->getBBox();

	remove_if(pa.begin(), pa.end(), [bboxB](const vec4& v) { return !bboxB.isInside(v); });
	remove_if(pb.begin(), pb.end(), [bboxA](const vec4& v) { return !bboxA.isInside(v); });

	// both points should now be valid and overlapping
	*/

	// 5) reduce the points again for sparceICP
	source.resize(Eigen::NoChange, pa.size());
	for (size_t i = 0; i < pa.size(); ++i)
	{
		const glm::vec4& v = pa[i];
		source(0, i) = v.x;
		source(1, i) = v.y;
		source(2, i) = v.z;
	}

	target.resize(Eigen::NoChange, pb.size());
	for (size_t i = 0; i < pb.size(); ++i)
	{
		const glm::vec4& v = pb[i];
		target(0, i) = v.x;
		target(1, i) = v.y;
		target(2, i) = v.z;
	}
	
	std::cout << "[Fit] Running icp ...";
	auto tic = std::chrono::steady_clock::now();
	
	SICP::Parameters pars;
	pars.p = 0.1;
	pars.max_icp = 20;
	pars.print_icpn = true;
	SICP::point_to_point(source, target, pars);
	
	auto toc = std::chrono::steady_clock::now();
	std::cout << " done. Running time: " << std::chrono::duration<double, std::milli>(toc - tic).count() << " ms.\n";
	
	
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


	if (drawGrid)
	{
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

		
		const glm::vec3 camPos = vp.camera->getPosition();
		const glm::vec3 viewDir = glm::normalize(vp.camera->target - camPos);

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

			sprintf(uname, "volume[%d].enabled", i);
			volumeShader2->setUniform(uname, stacks[i]->isEnabled());

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
		
	if (drawBbox)
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
}

static void display()
{
		
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);	
	

	for (size_t i = 0; i < layout->getViewCount(); ++i)
	{
		const Viewport* vp = layout->getView(i);
		vp->setup();
		drawScene(*vp);
	}

	
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
		drawSlices = !drawSlices;

	if (key == 'n')
	{
		++currentStack;
		if (currentStack == stacks.size())
			currentStack = 0;

		std::cout << "Current stack: " << currentStack << std::endl;
	}

	if (key == 'g')
		drawGrid = !drawGrid;

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
	

	if (key == ' ' && stacks.size() >= 2)
	{
		alignStacks(stacks[0], stacks[1]);
		drawPoints = true;

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



	if (key == 'v' && currentStack > -1)
		stacks[currentStack]->toggle();
	

	// camera controls
	Viewport* vp = layout->getActiveViewport();
	
	if (vp)
	{
		if (key == '-')
			vp->camera->zoom(0.7f);
		if (key == '=')
			vp->camera->zoom(1.4f);

		if (key == 'w')
			vp->camera->pan(0.f, 10.f);
		if (key == 's')
			vp->camera->pan(0.f, -10.f);
		if (key == 'a')
			vp->camera->pan(-10.f, 0.f);
		if (key == 'd')
			vp->camera->pan(10.f, 0.f);


		if (key == 'c')
		{
			//if (view[vp].name == Viewport::PERSPECTIVE)
			{
				if (currentStack == -1)
					vp->camera->target = globalBBox.getCentroid();
				else
					vp->camera->target = stacks[currentStack]->getBBox().getCentroid();
			}


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


	const Viewport* v = layout->getActiveViewport();

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

	int h = glutGet(GLUT_WINDOW_HEIGHT);
	layout->updateMouseMove(glm::ivec2(x, h - y));
}

static void passiveMotion(int x, int y)
{

	int h = glutGet(GLUT_WINDOW_HEIGHT);
	layout->updateMouseMove(glm::ivec2(x, h-y));
}

static void special(int key, int x, int y)
{
	if (key == GLUT_KEY_F1)
	{
		int w = glutGet(GLUT_WINDOW_WIDTH);
		int h = glutGet(GLUT_WINDOW_HEIGHT);

		std::cout << "[Layout] Creating fullscreen perspective layout ... \n";
		delete layout;
		layout = new PerspectiveFullLayout(glm::ivec2(w, h));
		layout->updateMouseMove(mouse.coordinates);
	}

	if (key == GLUT_KEY_F2)
	{
		int w = glutGet(GLUT_WINDOW_WIDTH);
		int h = glutGet(GLUT_WINDOW_HEIGHT);

		std::cout << "[Layout] Creating fullscreen top-view layout ... \n";
		delete layout;
		layout = new TopViewFullLayout(glm::ivec2(w, h));;
		layout->updateMouseMove(mouse.coordinates);
	}


	if (key == GLUT_KEY_F3)
	{
		int w = glutGet(GLUT_WINDOW_WIDTH);
		int h = glutGet(GLUT_WINDOW_HEIGHT);

		std::cout << "[Layout] Creating four-view layout ... \n";
		delete layout;
		layout = new FourViewLayout(glm::ivec2(w, h));
		layout->updateMouseMove(mouse.coordinates);

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
	layout->resize(ivec2(w, h));
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
			
	try
	{
		globalBBox.reset();


		for (int i = 0; i < 1; ++i)
		{
			char filename[256];
			//sprintf(filename, "e:/spim/test/spim_TL00_Angle%d.tif", i);
			//sprintf(filename, "E:/spim/091015 SPIM various size beads/091015 20micron beads/spim_TL01_Angle%d.ome.tiff", i);
			//sprintf(filename, "e:/spim/zebra/spim_TL01_Angle%d.ome.tiff", i);

			sprintf(filename, "e:/spim/zebra_beads/spim_TL01_Angle%d.ome.tiff", i);

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


		// setup viewports
		layout = new PerspectiveFullLayout(glm::ivec2(1024, 768));

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

