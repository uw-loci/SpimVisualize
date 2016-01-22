
#include "SpimRegistrationApp.h"
#include "SimplePointcloud.h"

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
	

	// backspace
	if (key == '\b')
		regoApp->undoLastTransform();

	if (key == ' ')
		regoApp->beginAutoAlign();

	if (key == 'H')
		regoApp->clearHistory();
	
	if (key == ',')
		regoApp->decreaseMinThreshold();
	if (key == '.')
		regoApp->increaseMaxThreshold();
	/*
	if (key == '<')
		regoApp->decreaseMinThreshold();
	if (key == '>')
		regoApp->decreaseMaxThreshold();
	*/
	if (key == 'C')
		regoApp->autoThreshold();
	if (key == 't')
		regoApp->contrastEditorApplyThresholds();

	if (key == ';')
		regoApp->decreaseSliceCount();
	if (key == '\'')
		regoApp->increaseSliceCount();

	
	if (key == '[')
		regoApp->rotateCurrentStack(-0.5);

	if (key == ']')
		regoApp->rotateCurrentStack(0.5);
	
	if (key == 'r')
		regoApp->switchRenderMode();
	if (key == 'R')
		regoApp->switchBlendMode();

	if (key == 's')
		regoApp->reloadShaders();

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

	
	/*
	if (key == 't')
		regoApp->loadStackTransformations();

	if (key == 'T')
		regoApp->saveStackTransformations();
	*/

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
	if (key == 'c')
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
	

	int h = glutGet(GLUT_WINDOW_HEIGHT);
	
	if (mouse.button[1])
		regoApp->panCamera(glm::vec2(dx, dy));

	if (mouse.button[2])
		regoApp->rotateCamera(glm::vec2(dt, dp));

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
		regoApp->selectSolver("Uniform"); 
	if (key == GLUT_KEY_F10)
		regoApp->selectSolver("Simulated Annealing");
	if (key == GLUT_KEY_F11)
		regoApp->selectSolver("Hillclimb");

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

	if (state == GLUT_UP)
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
	regoApp->setConfigPath("e:/regoApp/");

	try
	{
		/*
		for (int i = 1; i < argc; ++i)
		{
			regoApp->addSpimStack(argv[i]);

		}
		*/

		/*
		SimplePointcloud::resaveAsBin("e:/urs/ES_20151111.txt");
		SimplePointcloud::resaveAsBin("e:/urs/ES_20151122.txt");

		regoApp->addPointcloud("e:/urs/ES_20151111.bin");
		regoApp->addPointcloud("e:/urs/ES_20151122.bin");
		*/

	
		//regoApp->addSpimStack("e:/spim/OpenSPIM_tutorial/spim_TL00_Angle1.ome.tiff");
		regoApp->addSpimStack("e:/spim/zebra/spim_TL01_Angle0.ome.tiff");
		regoApp->addSpimStack("e:/spim/zebra/spim_TL01_Angle1.ome.tiff");

		regoApp->centerCamera();
		regoApp->loadStackTransformations();
		regoApp->loadContrastSettings();
		
		regoApp->reloadShaders();

		/*
		regoApp->addSpimStack("e:/spim/test_beads/spim_TL01_Angle0.ome.tiff");
		regoApp->centerCamera(); 
		*/
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

