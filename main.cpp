
#include "SpimRegistrationApp.h"
#include "SimplePointcloud.h"
#include "SpimStack.h"

#include <GL/glew.h>
#include <GL/freeglut.h>

#include <iostream>
#include <vector>
#include <algorithm>

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
	
	regoApp->draw();
	
	glutSwapBuffers();

}

static void idle()
{

	unsigned int time = glutGet(GLUT_ELAPSED_TIME);
	static unsigned int oldTime = 0;

	float dt = (float)(time - oldTime) / 1000.f;
	oldTime = time;
	
	regoApp->update(dt);
	//regoApp->setCameraMoving(false);

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
	
	if (key == 'p')
		regoApp->togglePhantoms();
	if (key == 'P')
		regoApp->alignPhantoms();

	// backspace
	if (key == '\b')
		regoApp->undoLastTransform();

	if (key == ' ')
		regoApp->beginAutoAlign();
	
	if (key == '\t')
		regoApp->beginMultiAutoAlign();

	if (key == 'a')
		regoApp->runAlignmentOnce();


	if (key == 'h')
		regoApp->toggleHistory();

	if (key == 'H')
		regoApp->clearHistory();
	
	if (key == ',')
		regoApp->decreaseMinThreshold();
	if (key == '.')
		regoApp->increaseMinThreshold();

	if (key == '<')
		regoApp->decreaseMaxThreshold();
	if (key == '>')
		regoApp->increaseMaxThreshold();
	/*
	if (key == '<')
		regoApp->decreaseMinThreshold();
	if (key == '>')
		regoApp->decreaseMaxThreshold();
	*/
	if (key == 'C')
		regoApp->autoThreshold();
	
	if (key == 'c')
	{
		if (glutGetModifiers() == GLUT_ACTIVE_ALT)
			regoApp->contrastEditorResetThresholds();
		else
			regoApp->contrastEditorApplyThresholds();

	}

	if (key == ';')
		regoApp->decreaseSliceCount();
	if (key == '\'')
		regoApp->increaseSliceCount();

	
	if (key == '[')
		regoApp->rotateCurrentStack(-0.5);
	if (key == '{')
		regoApp->rotateCurrentStack(-2.5f);

	if (key == ']')
		regoApp->rotateCurrentStack(0.5);
	if (key == '}')
		regoApp->rotateCurrentStack(2.5f);
		
	if (key == 's')
		regoApp->reloadShaders();

	if (key == 'q')
		regoApp->toggleSolutionParameterSpace();
	if (key == 'Q')
		regoApp->clearSolutionParameterSpace();


	if (key == 'u')
		regoApp->subsampleAllStacks();

	if (key == 'Y')
		regoApp->clearRays();

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

	if (key == '6')
		regoApp->toggleSelectStack(5);


	if (key == '7')
		regoApp->toggleSelectStack(6);


	
	if (key == 'T')
		regoApp->loadStackTransformations();

	if (key == 't')
		regoApp->saveStackTransformations();
	

	if (key == 'm')
		regoApp->maximizeViews();


	if (key == 'v')
		regoApp->toggleCurrentStack();
	if (key == 'V')
		regoApp->toggleAllStacks();


	if (key == '-')
		regoApp->zoomCamera(0.7f);
	if (key == '=')
		regoApp->zoomCamera(1.4f);
	if (key == 'f')
		regoApp->centerCamera();

	/*
	if (key == 'w')
		regoApp->panCamera(glm::vec2(0, 10));
	if (key == 's')
		regoApp->panCamera(glm::vec2(0, -10));
	if (key == 'a')
		regoApp->panCamera(glm::vec2(-10, 0));
	if (key == 'd')
		regoApp->panCamera(glm::vec2(10, 0));
	*/
}

static void keyboardUp(unsigned char key, int x, int y)
{
	if (key == ' ')
		regoApp->endAutoAlign();


	if (key == '\t')
		regoApp->endAutoAlign(); 
}


static void motion(int x, int y)
{
	float dt = (mouse.coordinates.y - y) * 0.1f;
	float dp = (mouse.coordinates.x - x) * 0.1f;

	//camera.rotate(dt, dp);

	float dx = (x - mouse.coordinates.x) * 2.f;
	float dy = (y - mouse.coordinates.y) * -2.f;

	mouse.coordinates.x = x;
	mouse.coordinates.y = y;
	

	int h = glutGet(GLUT_WINDOW_HEIGHT);
	
	if (mouse.button[1])
		regoApp->panCamera(glm::vec2(dx, dy));

	if (mouse.button[2])
	{
		if (glutGetModifiers() & GLUT_ACTIVE_SHIFT)
			regoApp->panCamera(glm::vec2(dx, dy));
		else
			regoApp->rotateCamera(glm::vec2(dt, dp));


	}

	if (mouse.button[0])
	{
		// let the application decide what to do with it:
		// camera movement in a perspective window
		//regoApp->rotateCamera(glm::vec2(dt, dp));
		
		// stack movement in an ortho window
		regoApp->moveStack(glm::vec2(dx, dy));

		// change contrast in the editor
		regoApp->changeContrast(glm::ivec2(x, h - y));


		regoApp->inspectOutputImage(glm::ivec2(x, h - y));

	}

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

	if (key == GLUT_KEY_F4)
		regoApp->setContrastEditorLayout(winRes, mouse.coordinates);


	if (key == GLUT_KEY_F5)
		regoApp->selectSolver("Uniform DX");
	if (key == GLUT_KEY_F6)
		regoApp->selectSolver("Uniform DY");
	if (key == GLUT_KEY_F7)
		regoApp->selectSolver("Uniform DZ");
	if (key == GLUT_KEY_F8)
		regoApp->selectSolver("Uniform RY");
	if (key == GLUT_KEY_F9)
		regoApp->selectSolver("Simulated Annealing");
	if (key == GLUT_KEY_F10)
		regoApp->selectSolver("Hillclimb");
	if (key == GLUT_KEY_F11)
		regoApp->selectSolver("Solution Parameterspace");

	if (key == GLUT_KEY_UP)
	{
		float d = 1.f;

		if (glutGetModifiers() == GLUT_ACTIVE_SHIFT)
			d = 10.f;
		else if (glutGetModifiers() == GLUT_ACTIVE_ALT)
			d = 0.1f;

		regoApp->moveStack(glm::vec2(0, d));
	}

	if (key == GLUT_KEY_DOWN)
	{

		float d = 1.f;

		if (glutGetModifiers() == GLUT_ACTIVE_SHIFT)
			d = 10.f;
		else if (glutGetModifiers() == GLUT_ACTIVE_ALT)
			d = 0.1f;

		regoApp->moveStack(glm::vec2(0, -d));
	}

	if (key == GLUT_KEY_LEFT)
	{

		float d = 1.f;

		if (glutGetModifiers() == GLUT_ACTIVE_SHIFT)
			d = 10.f;
		else if (glutGetModifiers() == GLUT_ACTIVE_ALT)
			d = 0.1f;

		regoApp->moveStack(glm::vec2(-d, 0));

	}


	if (key == GLUT_KEY_RIGHT)
	{
		float d = 1.f;

		if (glutGetModifiers() == GLUT_ACTIVE_SHIFT)
			d = 10.f;
		else if (glutGetModifiers() == GLUT_ACTIVE_ALT)
			d = 0.1f;

		regoApp->moveStack(glm::vec2(d, 0));

	}
	
}

static void button(int button, int state, int x, int y)
{
	assert(regoApp);

	std::cout << "Button: " << button << " state: " << state << " x " << x << " y " << y << std::endl;

	mouse.coordinates.x = x;
	mouse.coordinates.y = y;

	mouse.button[button] = (state == GLUT_DOWN);

	if (button == 2 && state == GLUT_UP)
		regoApp->setCameraMoving(false);

	if (button == 0 && state == GLUT_DOWN)
		regoApp->startStackMove();
	if (button == 0 && state == GLUT_UP)
		regoApp->endStackMove();
}

static void reshape(int w, int h)
{
	regoApp->resize(glm::ivec2(w, h));
}

static void cleanup()
{

	try
	{
		regoApp->saveStackTransformations();
		regoApp->saveContrastSettings();
	}
	catch (std::runtime_error& e)
	{
		std::cerr << "[Error] " << e.what() << std::endl;
	}

	delete regoApp;
}

int main(int argc, const char** argv)
{
	/*
	if (argc < 2)
	{
		std::cerr << "[Error] No filename given!\n";
		std::cerr << "[Usage] " << argv[0] << " <spimfile>\n";
		return -1;
	}
	*/

	
	glutInit(&argc, const_cast<char**>(argv));
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
	glutInitWindowSize(1024, 768);
	glutCreateWindow("Geometry Image");
	glewInit();

	glutDisplayFunc(display);
	glutIdleFunc(idle);
	glutMouseFunc(button);
	glutKeyboardFunc(keyboard);
	glutKeyboardUpFunc(keyboardUp);
	glutMotionFunc(motion);
	glutReshapeFunc(reshape);
	glutPassiveMotionFunc(passiveMotion);
	glutSpecialFunc(special);
	
	glEnable(GL_DEPTH_TEST);
	glPointSize(2.f);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
			

	glClampColor(GL_CLAMP_READ_COLOR, GL_FALSE);
	glClampColor(GL_CLAMP_VERTEX_COLOR, GL_FALSE);
	glClampColor(GL_CLAMP_FRAGMENT_COLOR, GL_FALSE);

	regoApp = new SpimRegistrationApp(glm::ivec2(1024,768));

#ifdef _WIN32
	regoApp->setConfigPath("e:/regoApp/");
#endif

	try
	{

		for (int i = 1; i < argc; ++i)
		{
			regoApp->addSpimStack(argv[i]);

		}


		/*
		SimplePointcloud::resaveAsBin("e:/urs/ES_20151111.txt");
		SimplePointcloud::resaveAsBin("e:/urs/ES_20151122.txt");

		regoApp->addPointcloud("e:/urs/ES_20151111.bin");
		regoApp->addPointcloud("e:/urs/ES_20151122.bin");
		*/








		// just to evaluate transformations
		SpimStack* reference = SpimStack::load("e:/spim/phantom/t1-head/t1-head.tiff");
		reference->setVoxelDimensions(glm::vec3(1, 1, 1));
		regoApp->addSpimStack(reference);

		SpimStack* test = SpimStack::load("e:/spim/phantom/t1-head/phantom_3.tiff");
		test->setVoxelDimensions(glm::vec3(1, 1, 1));
		regoApp->addSpimStack(test);


		regoApp->loadPrevSolutionSpace("e:/temp/output.csv");

		/*
		for (int i = 1; i < 6; ++i)
		{
			char filename[256];
			sprintf(filename, "e:/spim/phantom/phantom_%d.tiff", i);

			SpimStack* stack = SpimStack::load(filename);
			stack->setVoxelDimensions(glm::vec3(1.f, 1.f, 5.f));

			regoApp->addSpimStack(stack);

		}

		


		regoApp->addPhantom("e:/spim/phantom/phantom_1.tiff", "e:/spim/phantom/phantom_1.tiff.reference.txt");
		regoApp->addPhantom("e:/spim/phantom/phantom_2.tiff", "e:/spim/phantom/phantom_2.tiff.reference.txt");
		regoApp->addPhantom("e:/spim/phantom/phantom_3.tiff", "e:/spim/phantom/phantom_3.tiff.reference.txt");
		regoApp->addPhantom("e:/spim/phantom/phantom_4.tiff", "e:/spim/phantom/phantom_4.tiff.reference.txt");
		regoApp->addPhantom("e:/spim/phantom/phantom_5.tiff", "e:/spim/phantom/phantom_5.tiff.reference.txt");
		//regoApp->addPhantom("e:/spim/phantom/phantom_6.tiff", "e:/spim/phantom/phantom_6.tiff.reference.txt");
		*/


		regoApp->centerCamera();
		regoApp->loadStackTransformations();
		//regoApp->loadContrastSettings();
		
		regoApp->reloadShaders();

	}
	catch (const std::runtime_error& e)
	{
		std::cerr << "[Error] " << e.what() << std::endl;
	}
	catch (const std::string& e)
	{
		std::cerr << "[Error] " << e << std::endl;
	}

	atexit(cleanup);

	glutMainLoop();

#ifdef _WIN32
	system("pause");
#endif


	return 0;
}

