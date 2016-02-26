
#include "CreatePhantomApp.h"
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


CreatePhantomApp*	app = nullptr;


static void display()
{
		
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);	
	
	app->draw();
	
	glutSwapBuffers();

}

static void idle()
{

	unsigned int time = glutGet(GLUT_ELAPSED_TIME);
	static unsigned int oldTime = 0;

	float dt = (float)(time - oldTime) / 1000.f;
	oldTime = time;
	
	app->update(dt);
	//app->setCameraMoving(false);

	glutPostRedisplay();
}

static void keyboard(unsigned char key, int x, int y)
{
	if (key == 27)
		exit(0);

	if (key == '/')
		app->toggleSlices();

	if (key == 'g')
		app->toggleGrid();

	if (key == 'b')
		app->toggleBboxes();
	

	// backspace
	if (key == '\b')
		app->undoLastTransform();

	
	if (key == ',')
		app->decreaseMinThreshold();
	if (key == '.')
		app->increaseMaxThreshold();
	/*
	if (key == '<')
		app->decreaseMinThreshold();
	if (key == '>')
		app->decreaseMaxThreshold();
	*/
	if (key == 'C')
		app->autoThreshold();
	if (key == 't')
		app->contrastEditorApplyThresholds();


	if (key == ';')
		app->decreaseSliceCount();
	if (key == '\'')
		app->increaseSliceCount();

	
	if (key == '[')
		app->rotateCurrentStack(-0.5);
	if (key == '{')
		app->rotateCurrentStack(-5.f);

	if (key == ']')
		app->rotateCurrentStack(0.5);
	if (key == '}')
		app->rotateCurrentStack(5.f);
		
	if (key == 's')
		app->reloadShaders();

	if (key == 'u')
		app->subsampleAllStacks();


	if (key == 'e')
		app->createEmptyRandomStack();
	if (key == 'S')
		app->endSampleStack();

	if (key == '!')
		app->clearSampleStack();

	if (key == '@')
		app->startSampleStack(1);
	if (key == '#')
		app->startSampleStack(2);
	if (key == '$')
		app->startSampleStack(3);
	if (key == '%')
		app->startSampleStack(4);

	
	if (key == '1')
		app->toggleSelectStack(0);

	if (key == '2')
		app->toggleSelectStack(1);

	if (key == '3')
		app->toggleSelectStack(2);

	if (key == '4')
		app->toggleSelectStack(3);

	if (key == '5')
		app->toggleSelectStack(4);

	


	/*
	if (key == 't')
		app->loadStackTransformations();

	if (key == 'T')
		app->saveStackTransformations();
	*/

	if (key == 'm')
		app->maximizeViews();


	if (key == 'v')
		app->toggleCurrentStack();
	if (key == 'V')
		app->toggleAllStacks();


	if (key == '-')
		app->zoomCamera(0.7f);
	if (key == '=')
		app->zoomCamera(1.4f);
	if (key == 'c')
		app->centerCamera();

	/*
	if (key == 'w')
		app->panCamera(glm::vec2(0, 10));
	if (key == 's')
		app->panCamera(glm::vec2(0, -10));
	if (key == 'a')
		app->panCamera(glm::vec2(-10, 0));
	if (key == 'd')
		app->panCamera(glm::vec2(10, 0));
	*/
}

static void keyboardUp(unsigned char key, int x, int y)
{
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
		app->panCamera(glm::vec2(dx, dy));

	if (mouse.button[2])
		app->rotateCamera(glm::vec2(dt, dp));

	if (mouse.button[0])
	{
		// let the application decide what to do with it:
		// camera movement in a perspective window
		//app->rotateCamera(glm::vec2(dt, dp));
		
		// stack movement in an ortho window
		app->moveStack(glm::vec2(dx, dy));

		// change contrast in the editor
		app->changeContrast(glm::ivec2(x, h - y));


		app->inspectOutputImage(glm::ivec2(x, h - y));

	}

	app->updateMouseMotion(glm::ivec2(x, h - y));
}

static void passiveMotion(int x, int y)
{
	int h = glutGet(GLUT_WINDOW_HEIGHT);
	app->updateMouseMotion(glm::ivec2(x, h - y));
}

static void special(int key, int x, int y)
{
	int w = glutGet(GLUT_WINDOW_WIDTH);
	int h = glutGet(GLUT_WINDOW_HEIGHT);
	const glm::ivec2 winRes(w, h);

	if (key == GLUT_KEY_F1)
		app->setPerspectiveLayout(winRes, mouse.coordinates);

	if (key == GLUT_KEY_F2)
		app->setTopviewLayout(winRes, mouse.coordinates);
	
	if (key == GLUT_KEY_F3)
		app->setThreeViewLayout(winRes, mouse.coordinates);
	
	if (key == GLUT_KEY_F4)
		app->setContrastEditorLayout(winRes, mouse.coordinates);
	

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
		app->setCameraMoving(false);


	if (button == 0 && state == GLUT_DOWN)
		app->startStackMove();
	if (button == 0 && state == GLUT_UP)
		app->endStackMove();
}

static void reshape(int w, int h)
{
	app->resize(glm::ivec2(w, h));
}

static void cleanup()
{

	try
	{
		app->saveStackTransformations();
		app->saveContrastSettings();
	}
	catch (std::runtime_error& e)
	{
		std::cerr << "[Error] " << e.what() << std::endl;
	}

	delete app;
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

	app = new CreatePhantomApp(glm::ivec2(1024, 768));
	app->setConfigPath("e:/app/");

	try
	{
		app->addSpimStack("e:/spim/zebra/spim_TL01_Angle0.ome.tiff");
		
		app->centerCamera();
		app->loadStackTransformations();
		//app->loadContrastSettings();
		
		app->createEmptyRandomStack();

		app->reloadShaders();

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

