#include "CreatePhantomApp.h"

#include "Framebuffer.h"
#include "Layout.h"
#include "Shader.h"
#include "SpimStack.h"
#include "OrbitCamera.h"
#include "BeadDetection.h"
#include "SimplePointcloud.h"
#include "StackTransformationSolver.h"
#include "TinyStats.h"

#include <algorithm>
#include <iostream>
#include <fstream>
#include <random>
#include <thread>

#include <GL/glew.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/io.hpp>
#include <glm/gtx/transform2.hpp>
#include <glm/gtx/quaternion.hpp>


#include <FreeImage.h>

#include <GL/glut.h>

#include <boost/lexical_cast.hpp>

const unsigned int MIN_SLICE_COUNT = 20;
const unsigned int MAX_SLICE_COUNT = 1500;
const unsigned int STD_SLICE_COUNT = 100;


#define USE_RAYTRACER 
#define USE_GPU_SAMPLING

CreatePhantomApp::CreatePhantomApp(const glm::ivec2& res) : pointShader(nullptr), sliceShader(nullptr), volumeShader(nullptr),
volumeRaycaster(nullptr), drawQuad(nullptr), volumeDifferenceShader(nullptr), gpuStackSampler(nullptr), stackSamplerTarget(nullptr),
	tonemapper(nullptr), layout(nullptr),
	drawGrid(true), drawBboxes(false), drawSlices(false), sliceCount(100),
	configPath("./"), cameraMoving(false), histogramsNeedUpdate(false), minCursor(0.f), maxCursor(1.f),
	subsampleOnCameraMove(false), useImageAutoContrast(false), currentVolume(-1), drawPosition(nullptr)
{
	shaderPath = "../shaders/";

	globalBBox.reset();
	layout = new PerspectiveFullLayout(res);

	prevLayouts["Perspective"] = layout;

	globalThreshold.min = 100;
	globalThreshold.max = 130; // std::numeric_limits<unsigned short>::max();

	resetSliceCount();
	reloadShaders();

	volumeRenderTarget = new Framebuffer(1024, 1024, GL_RGBA32F, GL_FLOAT, 1, GL_NEAREST);

	rayStartTarget = new Framebuffer(1024, 1024, GL_RGBA32F, GL_FLOAT);

	

}

CreatePhantomApp::~CreatePhantomApp()
{
	delete volumeRenderTarget;
	delete rayStartTarget;

	delete drawQuad;
	delete pointShader;
	delete sliceShader;
	delete volumeShader;
	delete volumeDifferenceShader;
	delete volumeRaycaster;
	delete drawPosition;
	delete gpuStackSampler;
	delete stackSamplerTarget;

	for (size_t i = 0; i < stacks.size(); ++i)
		delete stacks[i];

	
	for (auto l = prevLayouts.begin(); l != prevLayouts.end(); ++l)
	{
		assert(l->second);
		delete l->second;
	}
}

void CreatePhantomApp::reloadShaders()
{
	
	int numberOfVolumes = std::max(1, (int)stacks.size());

	std::vector<std::pair<std::string, std::string> > defines;
	defines.push_back(std::make_pair("VOLUMES", boost::lexical_cast<std::string>(numberOfVolumes)));
	

	delete pointShader;
	pointShader = new Shader(shaderPath + "points2.vert", shaderPath + "points2.frag");

	delete sliceShader;
	sliceShader = new Shader(shaderPath + "slices.vert", shaderPath + "slices.frag");

	delete volumeShader;
	volumeShader = new Shader(shaderPath + "volume2.vert", shaderPath + "volume2.frag", defines);

	delete volumeRaycaster;
	volumeRaycaster = new Shader(shaderPath + "volumeRaycast.vert", shaderPath + "volumeRaycast.frag", defines);

	delete drawQuad;
	drawQuad = new Shader(shaderPath + "drawQuad.vert", shaderPath + "drawQuad.frag");

	delete volumeDifferenceShader;
	volumeDifferenceShader = new Shader(shaderPath + "volumeDist.vert", shaderPath + "volumeDist.frag", defines);

	delete tonemapper;
	tonemapper = new Shader(shaderPath + "drawQuad.vert", shaderPath + "tonemapper.frag", defines);

	delete drawPosition;
	drawPosition = new Shader(shaderPath + "drawPosition.vert", shaderPath + "drawPosition.frag");

	delete gpuStackSampler;
	gpuStackSampler = new Shader(shaderPath + "samplePlane.vert", shaderPath + "samplePlane.frag");

}

void CreatePhantomApp::draw()
{
	renderTargetReadbackCurrent = false;

	for (size_t i = 0; i < layout->getViewCount(); ++i)
	{
		const Viewport* vp = layout->getView((unsigned int)i);
		vp->setup();

		if (vp->name == Viewport::CONTRAST_EDITOR)
			drawContrastEditor(vp);
		else
		{

#ifdef USE_RAYTRACER

			initializeRayTargets(vp);
			raytraceVolumes(vp);

			drawTexturedQuad(volumeRenderTarget->getColorbuffer());
#else


			/// actual drawing block begins
			/// --------------------------------------------------------

			volumeRenderTarget->bind();
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
							

			glEnable(GL_BLEND);
			//glBlendEquation(GL_FUNC_ADD);
			//glBlendEquation(GL_MAX);
			glBlendEquationSeparate(GL_FUNC_ADD, GL_MAX);


			glBlendFunc(GL_SRC_ALPHA, GL_ONE);
			glBlendFunc(GL_ONE, GL_ONE);

			// align three-color view
			drawViewplaneSlices(vp, volumeDifferenceShader);
		
			// normal view
			//drawViewplaneSlices(vp, volumeShader);

			
			glDisable(GL_BLEND);
			

			//drawRays(vp);


			
			/// --------------------------------------------------------
			/// drawing block ends



			glDisable(GL_BLEND);
			glBlendEquation(GL_FUNC_ADD);
			
			volumeRenderTarget->disable();
			
			
			
			
			drawTonemappedQuad();
			//drawTexturedQuad(volumeRenderTarget->getColorbuffer());
#endif



			if (drawGrid)
				drawGroundGrid(vp);
			if (drawBboxes)
				drawBoundingBoxes();


			if (!stackSamples.empty())
				drawStackSamples();

		}

		vp->drawBorder();

	}
	
}

void CreatePhantomApp::saveStackTransformations() const
{
	std::for_each(stacks.begin(), stacks.end(), [](const SpimStack* s) 
	{ 
		std::string filename = s->getFilename() + ".registration.txt";
		s->saveTransform(filename); 
	});
}

void CreatePhantomApp::loadStackTransformations()
{
	std::cout << "[Debug] WARNING loadStackTransformations _will_ break if there are stacks and point clouds in one scene!\n";
	for (unsigned int i = 0; i < stacks.size(); ++i)
	{
		saveVolumeTransform(i);

		SpimStack* s = stacks[i];
		std::string filename = s->getFilename() + ".registration.txt";
		s->loadTransform(filename);
	};

	updateGlobalBbox();
}

void CreatePhantomApp::saveContrastSettings() const
{
	std::string filename(configPath + "contrast.txt");
	std::ofstream file(filename);

	if (file.is_open())
	{

		if (!file.is_open())
			throw std::runtime_error("Unable to open contrast file \"" + filename + "\"!");


		std::cout << "[File] Saving contrast settings to \"" << filename << "\"\n";

		file << "min: " << (int)globalThreshold.min << std::endl;
		file << "max: " << (int)globalThreshold.max << std::endl;
	}
	else
	{
		throw std::runtime_error("Unable to write contrast settings to file \"" + filename + "\"!");
	}
}
void CreatePhantomApp::loadContrastSettings()
{
	std::string filename(configPath + "contrast.txt");
	std::ifstream file(filename);

	// fail gracefully
	if (!file.is_open())
	{
		std::cout << "[Warning] Unable to load contrast settings from \"" << filename << "\", skipping.\n";
		return;
	}
	std::cout << "[File] Loading contrast settings from \"" << filename << "\"\n";

	std::string tmp;;
	file >> tmp >> globalThreshold.min;
	file >> tmp >> globalThreshold.max;

	std::cout << "[Contrast] Global threshold: " << (int)globalThreshold.min << "->" << (int)globalThreshold.max << std::endl;
}



void CreatePhantomApp::addSpimStack(const std::string& filename, const glm::vec3& voxelDimensions)
{
	SpimStack* stack = SpimStack::load(filename);
	stack->setVoxelDimensions(glm::vec3(voxelDimensions));
	this->addSpimStack(stack);
}

void CreatePhantomApp::addSpimStack(const std::string& filename)
{
	SpimStack* stack = SpimStack::load(filename);	
	this->addSpimStack(stack);
}


void CreatePhantomApp::addSpimStack(SpimStack* stack)
{
	stacks.push_back(stack);
	addInteractionVolume(stack);
	
}


void CreatePhantomApp::addInteractionVolume(InteractionVolume* v)
{
	interactionVolumes.push_back(v);

	AABB bbox = v->getTransformedBBox();

	if (interactionVolumes.size() == 1)
		globalBBox = bbox;
	else
		globalBBox.extend(bbox);
}

void CreatePhantomApp::drawTexturedQuad(unsigned int texture) const
{

	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);

	drawQuad->bind();
	drawQuad->setTexture2D("colormap", texture);

	glBegin(GL_QUADS);
	glVertex2i(0, 1);
	glVertex2i(0, 0);
	glVertex2i(1, 0);
	glVertex2i(1, 1);
	glEnd();

	drawQuad->disable();

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);

}


void CreatePhantomApp::drawTonemappedQuad()
{

	if (!renderTargetReadbackCurrent)
		readbackRenderTarget();


	// read back render target to determine largest and smallest value
	bool readback = true;
	glm::vec4 minVal(std::numeric_limits<float>::max());
	glm::vec4 maxVal(std::numeric_limits<float>::lowest());

	for (size_t i = 0; i < renderTargetReadback.size(); ++i)
	{
		minVal = glm::min(minVal, renderTargetReadback[i]);
		maxVal = glm::max(maxVal, renderTargetReadback[i]);
	}



	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);

	tonemapper->bind();
	tonemapper->setUniform("sliceCount", (float)sliceCount);
	tonemapper->setTexture2D("colormap", volumeRenderTarget->getColorbuffer());
	tonemapper->setUniform("minVal", minVal);
	tonemapper->setUniform("maxVal", maxVal);

	tonemapper->setUniform("minThreshold", (float)globalThreshold.min);
	tonemapper->setUniform("maxThreshold", (float)globalThreshold.max);

	glBegin(GL_QUADS);
	glVertex2i(0, 1);
	glVertex2i(0, 0);
	glVertex2i(1, 0);
	glVertex2i(1, 1);
	glEnd();

	tonemapper->disable();

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);

}




glm::vec3 CreatePhantomApp::getRandomColor(unsigned int n)
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


		for (unsigned int i = 0; i < (n + 1) * 2; ++i)
		{
			float r = (float)rand() / RAND_MAX;
			float g = (float)rand() / RAND_MAX;
			float b = 1.f - (r + g);
			pool.push_back(glm::vec3(r, g, b));
		}
	}

	return pool[n];
}

void CreatePhantomApp::resize(const glm::ivec2& newSize)
{
	layout->resize(newSize);
}


void CreatePhantomApp::setPerspectiveLayout(const glm::ivec2& res, const glm::ivec2& mouseCoords)
{
	std::cout << "[Layout] Creating fullscreen perspective layout ... \n";
	
	if (prevLayouts["Perspective"])
	{
		layout = prevLayouts["Perspective"];
		layout->resize(res);
	}
	else
	{
		layout = new PerspectiveFullLayout(res);
		prevLayouts["Perspective"] = layout;
	}
	
	layout->updateMouseMove(mouseCoords);
}


void CreatePhantomApp::setTopviewLayout(const glm::ivec2& res, const glm::ivec2& mouseCoords)
{
	std::cout << "[Layout] Creating fullscreen top-view layout ... \n";

	if (prevLayouts["TopView"])
	{
		layout = prevLayouts["TopView"];
		layout->resize(res);
	}
	else
	{
		layout = new TopViewFullLayout(res);
		prevLayouts["TopView"] = layout;
	}

	layout->updateMouseMove(mouseCoords);
}

void CreatePhantomApp::setThreeViewLayout(const glm::ivec2& res, const glm::ivec2& mouseCoords)
{
	std::cout << "[Layout] Creating four-view layout ... \n";
	if (prevLayouts["FourView"])
	{
		layout = prevLayouts["FourView"];
		layout->resize(res);
	}
	else
	{
		layout = new FourViewLayout(res);
		prevLayouts["FourView"] = layout;
	}

	layout->updateMouseMove(mouseCoords);
}


void CreatePhantomApp::setContrastEditorLayout(const glm::ivec2& res, const glm::ivec2& mouseCoords)
{
	
	std::cout << "[Layout] Creating contrast editor layout ... \n";
	if (prevLayouts["ContrastEditor"])
	{
		layout = prevLayouts["ContrastEditor"];
		layout->resize(res);
	}
	else
	{
		layout = new ContrastEditLayout(res);
		prevLayouts["ContrastEditor"] = layout;
	}

	layout->updateMouseMove(mouseCoords);
}

void CreatePhantomApp::centerCamera()
{
	Viewport* vp = layout->getActiveViewport();

	if (vp)
	{
		if (currentVolume == -1)
			vp->camera->target = globalBBox.getCentroid();
		else
			vp->camera->target = interactionVolumes[currentVolume]->getTransformedBBox().getCentroid();
	}
}


void CreatePhantomApp::zoomCamera(float z)
{
	Viewport* vp = layout->getActiveViewport();

	if (vp)
		vp->camera->zoom(z);

	setCameraMoving(true);

}

void CreatePhantomApp::panCamera(const glm::vec2& delta)
{
	layout->panActiveViewport(delta);
	setCameraMoving(true);

}

void CreatePhantomApp::rotateCamera(const glm::vec2& delta)
{
	Viewport* vp = layout->getActiveViewport();
	if (vp && vp->name == Viewport::PERSPECTIVE)
		vp->camera->rotate(delta.x, delta.y);

	setCameraMoving(true);

}


void CreatePhantomApp::updateMouseMotion(const glm::ivec2& cursor)
{
	layout->updateMouseMove(cursor);
}

void CreatePhantomApp::toggleSelectStack(int n)
{
	if (n >= interactionVolumes.size())
		currentVolume= -1;

	if (currentVolume == n)
		currentVolume = -1;
	else
		currentVolume = n;
}

void CreatePhantomApp::toggleStack(int n)
{
	if (n >= stacks.size() || n < 0)
		return;

	stacks[n]->toggle();
}

void CreatePhantomApp::rotateCurrentStack(float rotY)
{
	if (!currentVolumeValid())
		return;

	interactionVolumes[currentVolume]->rotate(glm::radians(rotY));
	updateGlobalBbox();
}



void CreatePhantomApp::moveStack(const glm::vec2& delta)
{
	if (!currentVolumeValid())
		return;


	Viewport* vp = layout->getActiveViewport();
	if (vp && vp->name != Viewport::CONTRAST_EDITOR)
	{
		//stacks[currentStack]->move(vp->camera->calculatePlanarMovement(delta));
		interactionVolumes[currentVolume]->move(vp->camera->calculatePlanarMovement(delta));
	}
}

void CreatePhantomApp::contrastEditorApplyThresholds()
{
	unsigned short newMin = (unsigned short)(globalThreshold.min + minCursor * globalThreshold.getSpread());
	unsigned short newMax = (unsigned short)(globalThreshold.min + maxCursor * globalThreshold.getSpread());

	globalThreshold.min = newMin;
	globalThreshold.max = newMax;

	minCursor = 0.f;
	maxCursor = 1.f;

	std::cout << "[Contrast] Set new global threshold to [" << newMin << "->" << newMax << "]\n";
	histogramsNeedUpdate = true;
}

void CreatePhantomApp::contrastEditorResetThresholds()
{

	globalThreshold.min = 0;

	// range depends on the type of first stack
	globalThreshold.max = 255;
	if (!stacks.empty() && stacks[0]->getBytesPerVoxel() == 2)
		globalThreshold.max = std::numeric_limits<unsigned short>::max();


	std::cout << "[Contrast] Resetting global threshold to [" << globalThreshold.min << " -> " << globalThreshold.max << "]\n";



	minCursor = 0.f;
	maxCursor = 1.f;
	histogramsNeedUpdate = true;

}


void CreatePhantomApp::changeContrast(const glm::ivec2& cursor)
{
	Viewport* vp = layout->getActiveViewport();
	if (vp && vp->name == Viewport::CONTRAST_EDITOR)
	{
		// find closest contrast slider
		
		glm::vec2 coords = vp->getRelativeCoords(cursor);

		if (coords.x < 0)
			coords.x = 0.f;
		if (coords.x > 1.f)
			coords.x = 1.f;

		//std::cout << "[Debug] Cursor: " << cursor << ", coords: " << coords << std::endl;

		static unsigned int lastValue = 0;
		float currentContrastCursor = coords.x;

		if (abs(currentContrastCursor - minCursor) < abs(currentContrastCursor - maxCursor))
		{
			// min bar is closer to cursor -- move this
			minCursor = currentContrastCursor;
		}
		else
		{
			// max bar is closer to cursor -- move this
			maxCursor = currentContrastCursor;
		}

		
		unsigned int value = (unsigned int)(globalThreshold.min + (int)(currentContrastCursor * globalThreshold.getSpread()));

		if (value != lastValue)
		{
			lastValue = value;
			std::cout << "[Debug] Selected Contrast: " << value << "(" << currentContrastCursor << ")\n";


			/*
			unsigned int index = (unsigned int)(currentContrastCursor * globalThreshold.getSpread());
			for (size_t i = 0; i < histograms.size(); ++i)
			{
				std::cout << "[Histo] " << i << ": " << histograms[i][index] << std::endl;
			}
			*/


		}


	}
}

void CreatePhantomApp::increaseMaxThreshold()
{
	globalThreshold.max += 5;
	std::cout << "[Threshold] Max: " << (int)globalThreshold.max << std::endl;

	histogramsNeedUpdate = true;
}

void CreatePhantomApp::increaseMinThreshold()
{
	globalThreshold.min += 5;
	if (globalThreshold.min > globalThreshold.max)
		globalThreshold.min = globalThreshold.max;
	std::cout << "[Threshold] Min: " << (int)globalThreshold.min << std::endl;

	histogramsNeedUpdate = true;
}

void CreatePhantomApp::decreaseMaxThreshold()
{
	globalThreshold.max -= 5;
	if (globalThreshold.max < globalThreshold.min)
		globalThreshold.max = globalThreshold.min;

	std::cout << "[Threshold] Max: " << (int)globalThreshold.max << std::endl;

	histogramsNeedUpdate = true;
}

void CreatePhantomApp::decreaseMinThreshold()
{
	globalThreshold.min -= 5;
	std::cout << "[Threshold] Min: " << (int)globalThreshold.min << std::endl;

	histogramsNeedUpdate = true;
}


void CreatePhantomApp::autoThreshold()
{
	using namespace std;

	globalThreshold.max = 0;
	globalThreshold.min = numeric_limits<double>::max();
	globalThreshold.mean = 0;
	globalThreshold.stdDeviation = 0;

	for (size_t i = 0; i < stacks.size(); ++i)
	{
		Threshold t = stacks[i]->getLimits();
		cout << "[Contrast] Stack " << i << " contrast: [" << t.min << " -> " << t.max << "], mean: " << t.mean << ", std dev: " << t.stdDeviation << std::endl;


		globalThreshold.min = min(globalThreshold.min, t.min);
		globalThreshold.max = max(globalThreshold.max, t.max);
		globalThreshold.mean += t.mean;
		globalThreshold.stdDeviation = max(globalThreshold.stdDeviation, t.stdDeviation);
	}

	globalThreshold.mean /= stacks.size();

	globalThreshold.min = (globalThreshold.mean - 3 * globalThreshold.stdDeviation);
	globalThreshold.min = std::max(globalThreshold.min, 0.0);

	globalThreshold.max = (globalThreshold.mean + 3 * globalThreshold.stdDeviation);

	cout << "[Contrast] Global contrast: [" << globalThreshold.min << " -> " << globalThreshold.max << "], mean: " << globalThreshold.mean << ", std dev: " << globalThreshold.stdDeviation << std::endl;



	histogramsNeedUpdate = true;
}

void CreatePhantomApp::toggleAllStacks()
{

	bool stat = !interactionVolumes[0]->enabled;
	for (size_t i = 0; i < interactionVolumes.size(); ++i)
		interactionVolumes[i]->enabled = stat;
}

void CreatePhantomApp::subsampleAllStacks()
{
	for (auto it = stacks.begin(); it != stacks.end(); ++it)
		(*it)->subsample(true);
}

void CreatePhantomApp::calculateHistograms()
{
	histograms.clear();

	size_t maxVal = 0;

	for (size_t i = 0; i < stacks.size(); ++i)
	{
		std::vector<size_t> histoRaw = stacks[i]->calculateHistogram(globalThreshold);

		for (size_t j = 0; j < histoRaw.size(); ++j)
			maxVal = std::max(maxVal, histoRaw[j]);


		std::vector<float> histoFloat(histoRaw.size());
		for (size_t j = 0; j < histoRaw.size(); ++j)
			histoFloat[j] = (float)histoRaw[j];

		//histoRaw.begin(), histoRaw.end());
		histograms.push_back(histoFloat);
	}

	std::cout << "[Contrast] Calculated " << histograms.size() << ", normalizing to " << maxVal << " ... \n";
		 

	// normalize based on max histogram value
	for (size_t i = 0; i < histograms.size(); ++i)
	{
		for (size_t j = 0; j < histograms[i].size(); ++j)
			histograms[i][j] /= maxVal;
	}



	histogramsNeedUpdate = false;
}


void CreatePhantomApp::drawContrastEditor(const Viewport* vp)
{
	if (histograms.empty() || histogramsNeedUpdate)
		calculateHistograms();
		
	glColor3f(1,1,1);
	glBegin(GL_LINE_LOOP);
	glVertex2i(0, 0);
	glVertex2i(1, 0);
	glVertex2i(1, 1);
	glVertex2i(0, 1);
	glEnd();



	const double leftLimit = globalThreshold.min;
	const double rightLimit = globalThreshold.max;


	glLineWidth(2.f);
	glBegin(GL_LINES);
	
	glColor3f(1, 0, 0);
	glVertex2f(maxCursor, 0.f);
	glVertex2f(maxCursor, 1.f);

	glColor3f(1.f, 1.f, 0.f);
	glVertex2d(minCursor, 0);
	glVertex2d(minCursor, 1);

	glColor3f(0.1f, 0.1f, 0.1f);
	glVertex2d(minCursor, 0);
	glColor3f(1, 1, 1);
	glVertex2d(maxCursor, 1);

	glEnd();
	
	glLineWidth(1.f);





	// draw histogram
	
	glBegin(GL_LINES);
	for (size_t i = 0; i < histograms.size(); ++i)
	{
		glm::vec3 rc = getRandomColor((unsigned int)i);
		glColor3fv(glm::value_ptr(rc));
	

		for (unsigned int ix = 0; ix < histograms[i].size(); ++ix)
		{
			float x = (float)ix / histograms[i].size();

			// offset each value slightly
			//x += ((float)histograms.size() / (float)vp->size.x);
			x += 0.1f;

			glVertex2f(x, 0.f);
			glVertex2f(x, histograms[i][ix]);
		}



	}
	glEnd();

}


void CreatePhantomApp::drawBoundingBoxes() const
{
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);

	glActiveTexture(GL_TEXTURE0);
	glDisable(GL_BLEND);

	glEnableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);


	for (size_t i = 0; i < stacks.size(); ++i)
	{
		if (stacks[i]->enabled)
		{
			glPushMatrix();
			glMultMatrixf(glm::value_ptr(stacks[i]->getTransform()));


			if (i == currentVolume)
				glColor3f(1, 1, 0);
			else
				glColor3f(0.6f, 0.6f, 0.6f);
			stacks[i]->getBBox().draw();

			glPopMatrix();
		}
	}


	glColor3f(0, 1, 1);
	globalBBox.draw();

	glDisableClientState(GL_VERTEX_ARRAY);
	glDepthMask(GL_TRUE);
	glEnable(GL_DEPTH_TEST);
}

void CreatePhantomApp::drawGroundGrid(const Viewport* vp) const
{
	glColor4f(0.3f, 0.3f, 0.3f, 1.f);


	if (vp->name == Viewport::PERSPECTIVE || vp->name == Viewport::ORTHO_Y)
	{
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

	if (vp->name == Viewport::ORTHO_X)
	{
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

	if (vp->name == Viewport::ORTHO_Z)
	{
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

void CreatePhantomApp::drawViewplaneSlices(const Viewport* vp, const Shader* shader) const
{
	glm::mat4 mvp;// = vp.proj * vp.view;
	vp->camera->getMVP(mvp);
	
	shader->bind();

	const glm::vec3 camPos = vp->camera->getPosition();
	const glm::vec3 viewDir = glm::normalize(vp->camera->target - camPos);

	// the smallest and largest projected bounding box vertices -- used to calculate the extend of planes
	// to draw
	glm::vec3 minPVal(std::numeric_limits<float>::max()), maxPVal(std::numeric_limits<float>::lowest());

	for (size_t i = 0; i < stacks.size(); ++i)
	{
		if (!stacks[i]->enabled)
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

	shader->setUniform("minThreshold", (float)globalThreshold.min);
	shader->setUniform("maxThreshold", (float)globalThreshold.max);
	shader->setUniform("sliceCount", (float)sliceCount);
	shader->setUniform("stdDev", (float)globalThreshold.stdDeviation);

	// reorder stacks so that the current volume is always at index 0
	std::vector<SpimStack*> reorderedStacks(stacks);
	
	if (currentVolume > 0)
		std::rotate(reorderedStacks.begin(), reorderedStacks.begin() + currentVolume, reorderedStacks.end());
	
	for (size_t i = 0; i < reorderedStacks.size(); ++i)
	{	

		glActiveTexture((GLenum)(GL_TEXTURE0 + i));
		glBindTexture(GL_TEXTURE_3D, reorderedStacks[i]->getTexture());

#ifdef _WIN32
		char uname[256];
		sprintf_s(uname, "volume[%d].texture", i);
		shader->setUniform(uname, (int)i);

		AABB bbox = reorderedStacks[i]->getBBox();
		sprintf_s(uname, "volume[%d].bboxMax", i);
		shader->setUniform(uname, bbox.max);
		sprintf_s(uname, "volume[%d].bboxMin", i);
		shader->setUniform(uname, bbox.min);

		sprintf_s(uname, "volume[%d].enabled", i);
		shader->setUniform(uname, reorderedStacks[i]->enabled);

		sprintf_s(uname, "volume[%d].inverseMVP", i);
		shader->setMatrix4(uname, glm::inverse(mvp * reorderedStacks[i]->getTransform()));

#else
		char uname[256];
		sprintf(uname, "volume[%d].texture", i);
		shader->setUniform(uname, (int)i);

		AABB bbox = reorderedStacks[i]->getBBox();
		sprintf(uname, "volume[%d].bboxMax", i);
		shader->setUniform(uname, bbox.max);
		sprintf(uname, "volume[%d].bboxMin", i);
		shader->setUniform(uname, bbox.min);

		sprintf(uname, "volume[%d].enabled", i);
		shader->setUniform(uname, stacks[i]->enabled);

		sprintf(uname, "volume[%d].inverseMVP", i);
		shader->setMatrix4(uname, glm::inverse(mvp * reorderedStacks[i]->transform));
#endif
	}

	// draw all slices
	glBegin(GL_QUADS);	
	for (unsigned int z = 0; z < sliceCount; ++z)
	{
		// render back-to-front
		float zf = glm::mix(maxPVal.z, minPVal.z, (float)z / sliceCount);

		glVertex3f(minPVal.x, maxPVal.y, zf);
		glVertex3f(minPVal.x, minPVal.y, zf);
		glVertex3f(maxPVal.x, minPVal.y, zf);
		glVertex3f(maxPVal.x, maxPVal.y, zf);
	}
	glEnd();


	glActiveTexture(GL_TEXTURE0);

	shader->disable();
	
}

void CreatePhantomApp::drawAxisAlignedSlices(const glm::mat4& mvp, const glm::vec3& viewAxis, const Shader* shader) const
{
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_ONE, GL_ONE); // GL_ONE_MINUS_SRC_ALPHA);
	//glBlendFunc(GL_SRC_ALPHA, GL_ONE);

	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);

	shader->bind();
	shader->setUniform("mvpMatrix", mvp);

	shader->setUniform("maxThreshold", (int)globalThreshold.max);
	shader->setUniform("minThreshold", (int)globalThreshold.min);

	for (size_t i = 0; i < stacks.size(); ++i)
	{
		if (stacks[i]->enabled)
		{
			shader->setMatrix4("transform", stacks[i]->getTransform());
			
			stacks[i]->drawSlices(volumeShader, viewAxis);
		}


	}

	shader->disable();

	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
}

void CreatePhantomApp::drawAxisAlignedSlices(const Viewport* vp, const Shader* shader) const
{
	glm::mat4 mvp;// = vp.proj * vp.view;
	vp->camera->getMVP(mvp);

	// additive blending
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);

	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);

	shader->bind();
	shader->setUniform("mvpMatrix", mvp);

	shader->setUniform("maxThreshold", (float)globalThreshold.max);
	shader->setUniform("minThreshold", (float)globalThreshold.min);

	for (size_t i = 0; i < stacks.size(); ++i)
	{
		if (stacks[i]->enabled)
		{
			shader->setMatrix4("transform", stacks[i]->getTransform());
			
			// calculate view vector in volume coordinates
			glm::vec3 view = vp->camera->getViewDirection();
			glm::vec4(localView) = stacks[i]->getInverseTransform() * glm::vec4(view, 0.0);
			view = glm::vec3(localView);

			stacks[i]->drawSlices(volumeShader, view);
			//stacks[i]->drawSlices(volumeShader, glm::vec3(0,0,1));
		
		}


	}

	shader->disable();

	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
}



void CreatePhantomApp::raytraceVolumes(const Viewport* vp) const
{
	glm::mat4 mvp(1.f);
	vp->camera->getMVP(mvp);

	glm::mat4 imvp = glm::inverse(mvp);

	volumeRenderTarget->bind();
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	volumeRaycaster->bind();
	
	// bind all the volumes
	for (size_t i = 0; i < stacks.size(); ++i)
	{
		glActiveTexture((GLenum)(GL_TEXTURE0 + i));
		glBindTexture(GL_TEXTURE_3D, stacks[i]->getTexture());
		
#ifdef _WIN32

		char uname[256];
		sprintf_s(uname, "volume[%d].texture", i);
		volumeRaycaster->setUniform(uname, (int)i);

		sprintf_s(uname, "volume[%d].transform", i);
		volumeRaycaster->setMatrix4(uname, stacks[i]->getTransform());

		sprintf_s(uname, "volume[%d].inverseTransform", i);
		volumeRaycaster->setMatrix4(uname, stacks[i]->getInverseTransform());

		AABB bbox = stacks[i]->getBBox();
		sprintf_s(uname, "volume[%d].bboxMax", i);
		volumeRaycaster->setUniform(uname, bbox.max);
		sprintf_s(uname, "volume[%d].bboxMin", i);
		volumeRaycaster->setUniform(uname, bbox.min);


#else
		char uname[256];
		sprintf(uname, "volume[%d].texture", i);
		volumeRaycaster->setUniform(uname, (int)i);

		AABB bbox = stacks[i]->getBBox();
		sprintf(uname, "volume[%d].bboxMax", i);
		volumeRaycaster->setUniform(uname, bbox.max);
		sprintf(uname, "volume[%d].bboxMin", i);
		volumeRaycaster->setUniform(uname, bbox.min);

		sprintf(uname, "volume[%d].enabled", i);
		volumeRaycaster->setUniform(uname, stacks[i]->enabled);

		sprintf(uname, "volume[%d].inverseMVP", i);
		volumeRaycaster->setMatrix4(uname, glm::inverse(mvp * stacks[i]->transform));

		sprintf(uname, "volume[%d].transform", i);
		volumeRaycaster->setMatrix4(uname, stacks[i]->transform);

		sprintf(uname, "volume[%d].inverseTransform", i);
		volumeRaycaster->setMatrix4(uname, glm::inverse(stacks[i]->transform));
#endif
	}

	// set the global contrast
	volumeRaycaster->setUniform("minThreshold", (float)globalThreshold.min);
	volumeRaycaster->setUniform("maxThreshold", (float)globalThreshold.max);

	volumeRaycaster->setUniform("activeVolume", currentVolume);

	// bind the ray start/end textures
	volumeRaycaster->setTexture2D("rayStart", rayStartTarget->getColorbuffer(), (int)stacks.size());
	volumeRaycaster->setMatrix4("inverseMVP", imvp);

	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);

	// draw a screen-filling quad, tex coords will be calculated in shader
	glBegin(GL_QUADS);
	glVertex2i(0, 0);
	glVertex2i(1, 0);
	glVertex2i(1, 1);
	glVertex2i(0, 1);
	glEnd();
	
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);

	volumeRaycaster->disable();
	volumeRenderTarget->disable();
}


void CreatePhantomApp::undoLastTransform()
{
	if (transformUndoChain.empty())
		return;

	
	VolumeTransform vt = transformUndoChain.back();
	vt.volume->setTransform(vt.matrix);

	transformUndoChain.pop_back();
	updateGlobalBbox();
}

void CreatePhantomApp::startStackMove()
{
	if (!currentVolumeValid())
		return;

	//saveStackTransform(currentStack);
	saveVolumeTransform(currentVolume);
}

void CreatePhantomApp::endStackMove()
{
	updateGlobalBbox();




}

void CreatePhantomApp::saveVolumeTransform(unsigned int n)
{
	assert(n < interactionVolumes.size());

	VolumeTransform vt;
	vt.matrix = interactionVolumes[n]->getTransform();
	vt.volume = interactionVolumes[n];

	transformUndoChain.push_back(vt);
}

void CreatePhantomApp::updateGlobalBbox()
{
	if (interactionVolumes.empty())
	{
		globalBBox.reset();
		return;
	}
	
	globalBBox = interactionVolumes[0]->getTransformedBBox();
	for (size_t i = 0; i < interactionVolumes.size(); ++i)
		globalBBox.extend(interactionVolumes[i]->getTransformedBBox());
}

void CreatePhantomApp::update(float dt)
{
	using namespace std;
	
	static float time = 2.f;
	time -= dt;
	
	if (time <= 0.f && useImageAutoContrast)
	{
		time = 2.f;

		// TODO: change me to the _correct_ render target
		volumeRenderTarget->bind();
		glReadBuffer(GL_COLOR_ATTACHMENT0);

		std::vector<glm::vec4> pixels(volumeRenderTarget->getWidth()*volumeRenderTarget->getHeight());
		glReadPixels(0, 0, volumeRenderTarget->getWidth(), volumeRenderTarget->getHeight(), GL_RGBA, GL_FLOAT, glm::value_ptr(pixels[0]));
		volumeRenderTarget->disable();

		calculateImageContrast(pixels);
	}

	if (sampleStack != -1)
		addStackSamples();

}

void CreatePhantomApp::maximizeViews()
{
	if (currentVolumeValid())
	{
		for (auto it = prevLayouts.begin(); it != prevLayouts.end(); ++it)
			it->second->maximizeView(interactionVolumes[currentVolume]->getBBox());

	}
	else
	{
		for (auto it = prevLayouts.begin(); it != prevLayouts.end(); ++it)
			it->second->maximizeView(globalBBox);
	}
	
	
}

void CreatePhantomApp::toggleSlices()
{
	drawSlices = !drawSlices;
	std::cout << "[Render] Drawing " << (drawSlices ? "slices" : "volumes") << std::endl;
}

void CreatePhantomApp::inspectOutputImage(const glm::ivec2& cursor)
{
	using namespace glm;

	// only work with fullscreen layouts for now 
	if (!layout->isSingleView())
		return;

	if (currentVolumeValid())
		return;


	if (!renderTargetReadbackCurrent)
		readbackRenderTarget();

	// calculate relative coordinates
	Viewport* vp = layout->getActiveViewport();
	if (vp)
	{
		vec2 relCoords = vp->getRelativeCoords(cursor);

		// calculate image coords and index
		ivec2 imgCoords(relCoords * vec2(volumeRenderTarget->getWidth(), volumeRenderTarget->getHeight()));

		// clamp
		imgCoords = max(imgCoords, ivec2(0));
		imgCoords = min(imgCoords, ivec2(volumeRenderTarget->getWidth() - 1, volumeRenderTarget->getHeight() - 1));

		size_t index = imgCoords.x + imgCoords.y * volumeRenderTarget->getWidth();

		//std::cout << "[Debug] " << cursor << " -> " << relCoords << " -> " << imgCoords << std::endl;
		std::cout << "[Image] " << renderTargetReadback[index] << std::endl;


		
		/*
		// shoot rays!
		relCoords *= 2.f;
		relCoords -= vec2(1.f);

		std::cout << "[Debug] CoordS: " << relCoords << std::endl;

		mat4 mvp;
		vp->camera->getMVP(mvp);

		Ray ray;
		ray.createFromFrustum(mvp, relCoords);
		rays.push_back(ray);
		
		
		if (pointclouds.size() > 0)
			inspectPointclouds(ray);

		*/
	}

	/*
	static unsigned oldTime = 0;
	// TODO: change me!
	unsigned time = glutGet(GLUT_ELAPSED_TIME);
	if (time - oldTime > 1000)
	{
		vec4 minVal(std::numeric_limits<float>::max());
		vec4 maxVal(std::numeric_limits<float>::lowest());

		for (size_t i = 0; i < pixels.size(); ++i)
		{
			minVal = min(minVal, pixels[i]);
			maxVal = max(maxVal, pixels[i]);

		}
		oldTime = time;

		std::cout << "[Image] " << minVal << " -> " << maxVal << std::endl;
	}
	*/



}

void CreatePhantomApp::increaseSliceCount()
{
	sliceCount = (unsigned int)(sliceCount * 1.4f);
	sliceCount = std::min(sliceCount, MAX_SLICE_COUNT);
	std::cout << "[Slices] Slicecount: " << sliceCount << std::endl;
}


void CreatePhantomApp::decreaseSliceCount()
{
	sliceCount = (unsigned int)(sliceCount / 1.4f);
	sliceCount = std::max(sliceCount, MIN_SLICE_COUNT);
	std::cout << "[Slices] Slicecount: " << sliceCount << std::endl;
}

void CreatePhantomApp::resetSliceCount()
{
	sliceCount = STD_SLICE_COUNT;
	std::cout << "[Slices] Slicecount: " << sliceCount << std::endl;
}


void CreatePhantomApp::calculateImageContrast(const std::vector<glm::vec4>& img)
{
	float mean = 0.f;

	minImageContrast = std::numeric_limits<float>::max();
	maxImageContrast = std::numeric_limits<float>::lowest();
	
	for (size_t i = 0; i < img.size(); ++i)
	{
		mean += img[i].r;

		minImageContrast = std::min(minImageContrast, img[i].r);
		maxImageContrast = std::max(maxImageContrast, img[i].r);
	}
	mean /= img.size();

	
	float variance = 0.f;
	for (size_t i = 0; i < img.size(); ++i)
	{
		float v = (img[i].r - mean);
		variance += (v*v);
	}
	
	variance /= img.size();

	float stdDev = sqrtf(variance);


	float high = mean + 3 * stdDev;
	float low = mean - 3 * stdDev;

	std::cout << "[Image] Auto-contrast: [" << minImageContrast << "->" << maxImageContrast << "], mean: " << mean << ", std dev: " << stdDev << std::endl;
	minImageContrast = low;
	maxImageContrast = high;
	std::cout << "[Image] Auto-contrast: [" << minImageContrast << "->" << maxImageContrast << "]\n";

}

void CreatePhantomApp::drawScoreHistory(const TinyHistory<double>& hist) const
{
	
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, 500, hist.min, hist.max, 0, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	size_t offset = 0;
	if (hist.history.size() > 500)
		offset = hist.history.size() - 500;


	glColor3f(1, 1, 0);
	glBegin(GL_LINE_STRIP);
	for (size_t i = offset; i < hist.history.size(); ++i)
	{
		double d = hist.history[i];

		glVertex2d((double)i-offset, d);
	}
	glEnd();


}

double CreatePhantomApp::calculateImageScore()
{
	if (!renderTargetReadbackCurrent)
		readbackRenderTarget();

	double value = 0;

	double validCount = 0;

	for (size_t i = 0; i < renderTargetReadback.size(); ++i)
	{
		glm::vec3 color(renderTargetReadback[i]);
		value += abs(color.r);

		if (renderTargetReadback[i].a > 0)
			++validCount;
	}

	value /= validCount;

	//std::cout << "[Image] Read back render target score: " << value << std::endl;
	return value;
}


void CreatePhantomApp::readbackRenderTarget()
{
	volumeRenderTarget->bind();
	glReadBuffer(GL_COLOR_ATTACHMENT0);

	const unsigned int res = volumeRenderTarget->getWidth()*volumeRenderTarget->getHeight();
	if (renderTargetReadback.size() != res)
		renderTargetReadback.resize(res);

	glReadPixels(0, 0, volumeRenderTarget->getWidth(), volumeRenderTarget->getHeight(), GL_RGBA, GL_FLOAT, glm::value_ptr(renderTargetReadback[0]));
	volumeRenderTarget->disable();

	renderTargetReadbackCurrent = true;
	glReadBuffer(GL_BACK);
}

void CreatePhantomApp::initializeRayTargets(const Viewport* vp)
{
	


	vp->camera->setup();
	glm::mat4 mvp;
	vp->camera->getMVP(mvp);

	drawPosition->bind();
	drawPosition->setMatrix4("mvp", mvp);

	glEnableClientState(GL_VERTEX_ARRAY);

	glEnable(GL_CULL_FACE);
	glFrontFace(GL_CCW);

	// draw back faces
	rayStartTarget->bind();
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glCullFace(GL_FRONT);
	
	globalBBox.drawSolid();

	rayStartTarget->disable();
	
	glCullFace(GL_BACK);
	glDisableClientState(GL_VERTEX_ARRAY);

	drawPosition->disable();

}

void CreatePhantomApp::createEmptyRandomStack(const glm::ivec3& resolution, const glm::vec3& voxelDimensions)
{
	assert(!stacks.empty());
	using namespace glm;

	auto seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
	auto rand = std::bind(std::uniform_real_distribution<float>(-1.f, 1.f), std::mt19937(seed));

	//const glm::ivec3 resolution(1300, 1000, 100);

	SpimStackU16* stack = new SpimStackU16;
	stack->setVoxelDimensions(voxelDimensions);
	stack->setContent(resolution, 0);

	stacks.push_back(stack);
	addInteractionVolume(stack);
	saveVolumeTransform(stacks.size() - 1);
	
	// copy the transform of the base stack
	stack->setTransform(stacks[0]->getTransform());

	// create random translation
	vec3 delta(rand(), rand(), rand());

	delta *= vec3(1000, 80, 1000);
	stack->move(delta);

	
	// create random rotation
	float a = rand() * 25.f;
	stack->setRotation(a);
	
	std::cout << "[App] Created random stack with dT " << delta << " and R=" << a << std::endl;

}




void CreatePhantomApp::drawStackSamples() const
{
	using namespace glm;

	const vec3 red(1.f, 0.f, 0.f);
	const vec3 grn(0.f, 0.8f, 0.f); 

	glColor3f(1, 1, 1);
	glBegin(GL_POINTS);
	for (size_t i = 0; i < stackSamples.size(); ++i)
	{
		float a = stackSamples[i].a / 120.0f;

		vec3 color = mix(red, grn, a);

		glColor3fv(value_ptr(color));
		glVertex3fv(value_ptr(stackSamples[i]));

	}
	glEnd();
}

void CreatePhantomApp::addStackSamples()
{
	if (sampleStack == -1)
		return;



	SpimStack* stack = stacks[sampleStack];

	if (lastStackSample >= stack->getDepth() || lastStackSample < 0)
		return;
	
	//std::cout << "[Sampling] Rendering slice " << lastStackSample << " ... \n";

	stackSamplerTarget->bind();
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	gpuStackSampler->bind();
	
	// set sampling volume
	gpuStackSampler->setUniform("planeTransform", stack->getTransform());
	gpuStackSampler->setUniform("zPlane", (float)lastStackSample);
	gpuStackSampler->setUniform("planeResolution", (float)stack->getWidth(), (float)stack->getHeight());
	gpuStackSampler->setUniform("planeScale", stack->getVoxelDimensions());

	// set reference volume
	gpuStackSampler->setUniform("inverseVolumeTransform", stacks[0]->getInverseTransform());
	gpuStackSampler->setUniform("volumeScale", stacks[0]->getVoxelDimensions());
	gpuStackSampler->setUniform("volumeResolution", stacks[0]->getWidth(), stacks[0]->getHeight(), stacks[0]->getDepth());




	// draw the correct z-plane
	glBegin(GL_QUADS);
	glVertex2i(0, 0);
	glVertex2i(1, 0);
	glVertex2i(1, 1);
	glVertex2i(0, 1);
	glEnd();


	gpuStackSampler->disable();
	stackSamplerTarget->disable();
		

	// read back
	stackSamplerTarget->bind();
	std::vector<glm::vec4> sliceSamples(stack->getWidth()*stack->getHeight());
	glReadBuffer(GL_COLOR_ATTACHMENT0);

	glReadPixels(0, 0, stackSamplerTarget->getWidth(), stackSamplerTarget->getHeight(), GL_RGBA, GL_FLOAT, glm::value_ptr(sliceSamples[0]));

	stackSamplerTarget->disable();
	glReadBuffer(GL_BACK);



	// set the sample
	std::vector<double> samples(sliceSamples.size());
	for (size_t i = 0; i < samples.size(); ++i)
		samples[i] = sliceSamples[i].a;

	stack->setPlaneSamples(samples, lastStackSample);
	

	//fill in the correct slice of points
	//stackSamples.insert(stackSamples.end(), sliceSamples.begin(), sliceSamples.end());
	
	stackSamples = sliceSamples;

	
	++lastStackSample;
	if (lastStackSample == stack->getDepth())
	{
		stack->update();
		endSampleStack();
		

		stackSamples.clear();
	}

}


void CreatePhantomApp::startSampleStack(int n)
{
	if (n == 0 || n >= stacks.size())
	{
		std::cout << "[Sample] Invalid target stack: " << n << std::endl;
		return;
	}

	clearSampleStack();
	sampleStack = n;
	std::cout << "[Sample] Selecting stack " << sampleStack << " for sampling.\n";

	stackSamples.reserve(stacks[n]->getPlanePixelCount());

	delete stackSamplerTarget;
	stackSamplerTarget = new Framebuffer(stacks[n]->getWidth(), stacks[n]->getHeight(), GL_RGBA32F, GL_FLOAT);


}

void CreatePhantomApp::endSampleStack()
{
	

	// save the created stack
	const std::string savePath = "e:/spim/phantom/";

	std::string filename = savePath + "phantom_" + std::to_string(sampleStack);;
	stacks[sampleStack]->save(filename + ".tiff");
	stacks[sampleStack]->saveTransform(filename + ".transform.txt");
	

	sampleStack = -1;
	lastStackSample = 0;

}

void CreatePhantomApp::clearSampleStack()
{
	stackSamples.clear();
	lastStackSample = 0;
}

void CreatePhantomApp::deleteSelectedStack()
{
	if (currentVolume == -1)
	{
		std::cout << "[Stack] No valid stack selected.\n";
		return;
	}

	if (currentVolume == 0)
	{
		std::cout << "[Stack] Cannot delete the first stack.\n";
		return;
	}

	std::cout << "[Stack] Deleting volume " << currentVolume << std::endl;
	InteractionVolume* s = stacks[currentVolume];
	
	stacks.erase(stacks.begin() + currentVolume);
	interactionVolumes.erase(interactionVolumes.begin() + currentVolume);

	// also remove all transformation history containing this stack
	auto it = transformUndoChain.begin();
	while (it != transformUndoChain.end())
	{
		if (it->volume == s)
			transformUndoChain.erase(it);
		else
			++it;
	}


	delete s;
	updateGlobalBbox();
		
	currentVolume = -1;
}