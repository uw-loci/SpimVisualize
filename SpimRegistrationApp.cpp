#include "SpimRegistrationApp.h"

#include "Framebuffer.h"
#include "Layout.h"
#include "Shader.h"
#include "SpimStack.h"
#include "OrbitCamera.h"
#include "BeadDetection.h"
#include "SimplePointcloud.h"
#include "StackTransformationSolver.h"
#include "TinyStats.h"
#include "Widget.h"

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



#include <GL/glut.h>

#include <boost/lexical_cast.hpp>

const unsigned int MIN_SLICE_COUNT = 20;
const unsigned int MAX_SLICE_COUNT = 1500;
const unsigned int STD_SLICE_COUNT = 100;


#define USE_RAYTRACER 
#define USE_POINTCLOUDS

SpimRegistrationApp::SpimRegistrationApp(const glm::ivec2& res) : layout(nullptr), shaderPath("./shaders/"),
	histogramsNeedUpdate(false), minCursor(0.f), maxCursor(1.f), gridOffset(0.f),
	cameraMoving(false), drawGrid(false), drawBboxes(false), drawSlices(false), currentVolume(-1), sliceCount(100), subsampleOnCameraMove(false),
	pointShader(nullptr), volumeShader(nullptr), sliceShader(nullptr),
	volumeRaycaster(nullptr), drawQuad(nullptr), volumeDifferenceShader(nullptr), drawPosition(nullptr), tonemapper(nullptr), gpuStackSampler(nullptr),
	volumeRenderTarget(nullptr), rayStartTarget(nullptr), stackSamplerTarget(nullptr), pointSpriteShader(nullptr), gpuMultiStackSampler(nullptr),
	useImageAutoContrast(false), runAlignment(false), renderTargetReadbackCurrent(false), calculateScore(false), drawHistory(false),
	solver(nullptr), drawPhantoms(false), drawSolutionSpace(false), runAlignmentOnlyOncePlease(false),
	controlWidget(nullptr), pointSpriteTexture(0), cameraAutoRotate(false)
{

	config.setDefaults();

	globalBBox.reset();


	layout = new SwitchableLayoutContainer(new PerspectiveFullLayout(res), new FourViewLayout(res));
	prevLayouts["MainView"] = layout;
		
	resetSliceCount();
	
	volumeRenderTarget = new Framebuffer(1024, 1024, GL_RGBA32F, GL_FLOAT);

	rayStartTarget = new Framebuffer(512, 512, GL_RGBA32F, GL_FLOAT);


	createPointSpriteTexture();

	calculateScore = true;
	
	solver = new DXSolver;
	//solver = new SimulatedAnnealingSolver;

	setWidgetType("None");
}

SpimRegistrationApp::~SpimRegistrationApp()
{

	if (config.saveStackTransformationsOnExit)
	{
		try
		{
			this->saveStackTransformations();
		}
		catch (const std::runtime_error& err)
		{
			std::cerr << "[Error] " << err.what() << std::endl;
		}
	}

	delete solver;
	
	delete volumeRenderTarget;
	delete rayStartTarget;

	delete drawQuad;
	delete pointShader;
	delete sliceShader;
	delete volumeShader;
	delete volumeDifferenceShader;
	delete volumeRaycaster;
	delete drawPosition;
	delete stackSamplerTarget;
	delete pointSpriteShader;;
	delete gpuStackSampler;
	delete gpuMultiStackSampler;

	delete controlWidget;

	for (size_t i = 0; i < stacks.size(); ++i)
		delete stacks[i];

	for (size_t i = 0; i < pointclouds.size(); ++i)
		delete pointclouds[i];

	for (auto l = prevLayouts.begin(); l != prevLayouts.end(); ++l)
	{
		assert(l->second);
		delete l->second;
	}
}

void SpimRegistrationApp::reloadShaders()
{
	// these are all the shaders that depend on the number of volumes etc
	reloadVolumeShader();
	
	delete pointShader;
	pointShader = new Shader(shaderPath + "points2.vert", shaderPath + "points2.frag");

	delete sliceShader;
	sliceShader = new Shader(shaderPath + "slices.vert", shaderPath + "slices.frag");
		
	delete drawQuad;
	drawQuad = new Shader(shaderPath + "drawQuad.vert", shaderPath + "drawQuad.frag");
	
	delete drawPosition;
	drawPosition = new Shader(shaderPath + "drawPosition.vert", shaderPath + "drawPosition.frag");

	delete gpuStackSampler;
	gpuStackSampler = new Shader(shaderPath + "samplePlane.vert", shaderPath + "samplePlane.frag");

	delete pointSpriteShader;
	pointSpriteShader = new Shader(shaderPath + "pointsprite.vert", shaderPath + "pointsprite.frag");

}

void SpimRegistrationApp::reloadVolumeShader()
{
	int numberOfVolumes = std::max(1, (int)stacks.size());

	std::vector<std::pair<std::string, std::string> > defines;
	defines.push_back(std::make_pair("VOLUMES", boost::lexical_cast<std::string>(numberOfVolumes)));
	defines.push_back(std::make_pair("STEPS", boost::lexical_cast<std::string>(config.raytraceSteps)));



	delete tonemapper;
	tonemapper = new Shader(shaderPath + "drawQuad.vert", shaderPath + "tonemapper.frag", defines);

	delete volumeShader;
	volumeShader = new Shader(shaderPath + "volume2.vert", shaderPath + "volume2.frag", defines);

	delete volumeRaycaster;
	volumeRaycaster = new Shader(shaderPath + "volumeRaycast.vert", shaderPath + "volumeRaycast.frag", defines);

	delete volumeDifferenceShader;
	volumeDifferenceShader = new Shader(shaderPath + "volumeDist.vert", shaderPath + "volumeDist.frag", defines);
	
	delete gpuMultiStackSampler;
	gpuMultiStackSampler = new Shader(shaderPath + "samplePlane2.vert", shaderPath + "samplePlane2.frag", defines);
}


void SpimRegistrationApp::toggleRotateCamera()
{
	cameraAutoRotate = !cameraAutoRotate;
	cameraMoving = cameraAutoRotate;



}


void SpimRegistrationApp::draw()
{
	if (!pointShader)
		reloadShaders();

	renderTargetReadbackCurrent = false;

	for (size_t i = 0; i < layout->getViewCount(); ++i)
	{
		const Viewport* vp = layout->getView((unsigned int)i);
		vp->setup();

		/*
		if (vp->name == Viewport::CONTRAST_EDITOR)
			drawContrastEditor(vp);
		else
		*/
		{

			// apply new transform
			if (runAlignment)
			{

				try
				{
					const glm::mat4& mat = solver->getCurrentSolution().matrix;

					saveVolumeTransform(currentVolume);
					interactionVolumes[currentVolume]->applyTransform(mat);

					updateGlobalBbox();

				}
				catch (std::runtime_error& e)
				{
					std::cerr << "[Error] " << e.what() << std::endl;
				}
			}

#ifdef USE_POINTCLOUDS
			volumeRenderTarget->bind();
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			drawPointclouds(vp);
			volumeRenderTarget->disable();

			drawTexturedQuad(volumeRenderTarget->getColorbuffer());
#endif


#ifdef USE_RAYTRACER

			if (!stacks.empty())
			{
				initializeRayTargets(vp);
				raytraceVolumes(vp);

				drawTexturedQuad(volumeRenderTarget->getColorbuffer());
			}
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


			if (drawBboxes && drawPhantoms)
				drawPhantomBoxes();


			if (!stackSamples.empty())
				drawStackSamples();


			if (vp == layout->getActiveViewport())
			{

				if (drawSolutionSpace)
					drawSolutionParameterSpace(vp);

				if (controlWidget)
				{
					controlWidget->draw(vp);

					glMatrixMode(GL_PROJECTION);
					glLoadIdentity();
					glOrtho(0, vp->size.x, 0, vp->size.y, 0, 1);

					glMatrixMode(GL_MODELVIEW);
					glLoadIdentity();
					
					
					glRasterPos2i(10, 10);
					glColor4f(1, 1, 1, 1);
					
					
					std::string text;
					if (dynamic_cast<TranslateWidget*>(controlWidget))
						text = "Mode: Translation";
					else if (dynamic_cast<RotateWidget*>(controlWidget))
						text = "Mode: Rotation";
					else
						text = "Mode: Scale";

					text = text + " - " + controlWidget->getMode();

					for (int i = 0; i < text.length(); ++i)
						glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, text[i]);

				}


			}

			// the query result should be done by now
			if (runAlignment || calculateScore)
			{
				double score = calculateImageScore();

				//if (calculateScore)
				scoreHistory.add(score);

				if (runAlignment)
				{
					solver->recordCurrentScore(score);
					undoLastTransform();

				}
				
			}



		}

		vp->drawBorder();
		vp->drawTitle();

	}

	// draw the solver score here

	/*
	if (runAlignment || !solver->getHistory().history.empty())
		drawScoreHistory(solver->getHistory());
	*/

	if (calculateScore || runAlignment)
		drawScoreHistory(scoreHistory);
	


	
}

void SpimRegistrationApp::clearStackTransformations()
{
	std::for_each(interactionVolumes.begin(), interactionVolumes.end(), []( InteractionVolume* v)
	{
		v->setTransform(glm::mat4(1.f));
	});

}

void SpimRegistrationApp::saveStackTransformations()
{
	std::cout << "[Debug] Saving stack transformations.\n";

	std::for_each(stacks.begin(), stacks.end(), [=](const SpimStack* s)
	{
		std::string filename = s->getFilename() + ".registration.txt";
		s->saveTransform(filename);
	});

	
	std::for_each(pointclouds.begin(), pointclouds.end(), [=](const SimplePointcloud* p)
	{
		std::string filename = p->getFilename() + ".registration.txt";
		p->saveTransform(filename);
	});
}

void SpimRegistrationApp::loadStackTransformations()
{
	std::cout << "[Debug] WARNING loadStackTransformations _will_ break if there are stacks and point clouds in one scene!\n";
	for (unsigned int i = 0; i < stacks.size(); ++i)
	{
		saveVolumeTransform(i);

		SpimStack* s = stacks[i];
		
		std::string filename = s->getFilename() + ".registration.txt";
		if (!s->loadTransform(filename))
		{
			// if we are unable to load the registered transform, try to find the setting for it in the config file
			std::string filename = s->getFilename();
			filename = filename.substr(filename.find_last_of("\\")+1);


			std::cout << "[Debug] Looking for default initial placement for stack \"" << filename << "\" ... \n";

			for (size_t k = 0; k < config.stackPositions.size(); ++k)
			{
				const Config::InitialStackPosition& sp = config.stackPositions[k];
			
				if (sp.filename == filename)
				{
					std::cout << "[Debug] Found!\n";


					// multiply offset by 1000 to get from mm to nm
					glm::mat4 T = glm::translate(glm::vec3(sp.x, sp.y, sp.z) * 1000.f);
					glm::mat4 R = glm::rotate(glm::radians(sp.rotY), glm::vec3(0, 1, 0));

					s->setTransform(T*R);
					break;
				}
			
			}



			
		}

	};

	for (unsigned int i = 0; i < pointclouds.size(); ++i)
	{
		pointclouds[i]->loadTransform(pointclouds[i]->getFilename() + ".registration.txt");
	}

	

	updateGlobalBbox();
}

void SpimRegistrationApp::addSpimStack(const std::string& filename)
{

	SpimStack* stack = SpimStack::load(filename);
	stack->setVoxelDimensions(config.defaultVoxelSize);
	addSpimStack(stack);

}

void SpimRegistrationApp::addSpimStack(SpimStack* stack)
{
	stacks.push_back(stack);
	addInteractionVolume(stack);
}

void SpimRegistrationApp::addSpimStack(const std::string& filename, const glm::vec3& voxelScale)
{
	SpimStack* stack = SpimStack::load(filename);
	stack->setVoxelDimensions(voxelScale);
	addSpimStack(stack);
}

void SpimRegistrationApp::addPointcloud(const std::string& filename)
{
	const glm::mat4 scaleMatrix = glm::scale(glm::vec3(100.f));


	std::cout << "[File] Loading point cloud \"" << filename << "\" ... \n";
	SimplePointcloud* pc = new SimplePointcloud(filename, scaleMatrix);

	pointclouds.push_back(pc);
	addInteractionVolume(pc);
}

void SpimRegistrationApp::addInteractionVolume(InteractionVolume* v)
{
	interactionVolumes.push_back(v);

	AABB bbox = v->getTransformedBBox();

	if (interactionVolumes.size() == 1)
		globalBBox = bbox;
	else
		globalBBox.extend(bbox);
}

void SpimRegistrationApp::drawTexturedQuad(unsigned int texture) const
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


void SpimRegistrationApp::drawTonemappedQuad()
{

	if (!renderTargetReadbackCurrent)
		readbackRenderTarget();


	// read back render target to determine largest and smallest value
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

	tonemapper->setUniform("minThreshold", (float)config.threshold.min);
	tonemapper->setUniform("maxThreshold", (float)config.threshold.max);

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




glm::vec3 SpimRegistrationApp::getRandomColor(unsigned int n)
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

void SpimRegistrationApp::resize(const glm::ivec2& newSize)
{
	layout->resize(newSize);
}

void SpimRegistrationApp::setLayout(const std::string& name, const glm::ivec2& res, const glm::ivec2& mouseCoords)
{
	if (name == "MainView")
	{
		if (layout == prevLayouts["MainView"])
		{
			SwitchableLayoutContainer* slc = dynamic_cast<SwitchableLayoutContainer*>(layout);
			if (slc)
				slc->switchLayouts();
		}
		else
		{
			layout = prevLayouts["MainView"];
			layout->resize(res);
		}
	}
	else if (name == "OrthoY")
	{
		if (prevLayouts["OrthoY"])
		{
			layout = prevLayouts["OrthoY"];
			layout->resize(res);
		}
		else
		{
			/*
			layout = new TopViewFullLayout(res);
			prevLayouts["OrthoY"] = layout;
			*/
			layout = new OrthoViewFullLayout(res, "Ortho Y");
			prevLayouts["OrthoY"] = layout;
		}

	}
	else if (name == "OrthoX")
	{
		if (prevLayouts["OrthoX"])
		{
			layout = prevLayouts["OrthoX"];
			layout->resize(res);
		}
		else
		{
			layout = new OrthoViewFullLayout(res, "Ortho X");
			prevLayouts["OrthoX"] = layout;
		}
	}
	else if (name == "OrthoZ")
	{
		if (prevLayouts["OrthoZ"])
		{
			layout = prevLayouts["OrthoZ"];
			layout->resize(res);
		}
		else
		{
			layout = new OrthoViewFullLayout(res, "Ortho Z");
			prevLayouts["OrthoZ"] = layout;
		}
	}

	layout->updateMouseMove(mouseCoords);
}

void SpimRegistrationApp::centerCamera()
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


void SpimRegistrationApp::zoomCamera(float z)
{
	Viewport* vp = layout->getActiveViewport();

	if (vp)
		vp->camera->zoom(z);

	setCameraMoving(true);

}

void SpimRegistrationApp::panCamera(const glm::vec2& delta)
{
	layout->panActiveViewport(delta);
	setCameraMoving(true);

}

void SpimRegistrationApp::rotateCamera(const glm::vec2& delta)
{
	Viewport* vp = layout->getActiveViewport();
	if (vp && vp->name == "Perspective")
		vp->camera->rotate(delta.x, delta.y);

	setCameraMoving(true);

}


void SpimRegistrationApp::updateMouseMotion(const glm::ivec2& cursor)
{
	layout->updateMouseMove(cursor);
}

void SpimRegistrationApp::toggleSelectVolume(int n)
{
	if (n >= (int)interactionVolumes.size())
		currentVolume= -1;

	if (currentVolume == n)
		currentVolume = -1;
	else
		currentVolume = n;

	if (controlWidget)
		if (currentVolumeValid())
			controlWidget->setInteractionVolume(interactionVolumes[currentVolume]);
		else
			controlWidget->setInteractionVolume(nullptr);

}

void SpimRegistrationApp::toggleVolume(int n)
{
	if (n >= (int)interactionVolumes.size() || n < 0)
		return;

	interactionVolumes[n]->toggle();
}


void SpimRegistrationApp::toggleVolumeLock(int n)
{
	if (n >= (int)interactionVolumes.size() || n < 0)
		return;

	interactionVolumes[n]->toggleLock();
}

void SpimRegistrationApp::unlockAllVolumes()
{
	for (size_t i = 0; i < interactionVolumes.size(); ++i)
		interactionVolumes[i]->locked = false;
}


void SpimRegistrationApp::rotateCurrentVolume(float rotY)
{
	if (!currentVolumeValid())
		return;

	if (runAlignment)
		return;

	interactionVolumes[currentVolume]->rotate(glm::radians(rotY));
	updateGlobalBbox();
}

void SpimRegistrationApp::moveCurrentStack(const glm::vec2& delta)
{

	if (!currentVolumeValid())
		return;

	if (runAlignment)
		return;

	Viewport* vp = layout->getActiveViewport();
	if (vp && vp->name != "Histrogram")
	{
		//stacks[currentStack]->move(vp->camera->calculatePlanarMovement(delta));
		interactionVolumes[currentVolume]->move(vp->camera->calculatePlanarMovement(delta));
	}

	updateGlobalBbox();
}

void SpimRegistrationApp::contrastEditorApplyThresholds()
{
	unsigned short newMin = (unsigned short)(config.threshold.min + minCursor * config.threshold.getSpread());
	unsigned short newMax = (unsigned short)(config.threshold.min + maxCursor * config.threshold.getSpread());

	config.threshold.min = newMin;
	config.threshold.max = newMax;

	minCursor = 0.f;
	maxCursor = 1.f;

	std::cout << "[Contrast] Set new global threshold to [" << newMin << "->" << newMax << "]\n";
	histogramsNeedUpdate = true;
}

void SpimRegistrationApp::contrastEditorResetThresholds()
{
	
	config.threshold.min = 0;
	
	// range depends on the type of first stack
	config.threshold.max = 255;
	if (!stacks.empty() && stacks[0]->getBytesPerVoxel() == 2)
		config.threshold.max = std::numeric_limits<unsigned short>::max();


	std::cout << "[Contrast] Resetting global threshold to [" << config.threshold.min << " -> " << config.threshold.max << "]\n";



	minCursor = 0.f;
	maxCursor = 1.f;
	histogramsNeedUpdate = true;

}

void SpimRegistrationApp::changeContrast(const glm::ivec2& cursor)
{
	Viewport* vp = layout->getActiveViewport();
	if (vp && vp->name == "Histogram")
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

		
		unsigned int value = (unsigned int)(config.threshold.min + (int)(currentContrastCursor * config.threshold.getSpread()));

		if (value != lastValue)
		{
			lastValue = value;
			//std::cout << "[Debug] Selected Contrast: " << value << "(" << currentContrastCursor << ")\n";
			
			for (size_t i = 0; i < histograms.size(); ++i)
			{
				unsigned int index = currentContrastCursor * histograms[i].size();
				if (index == histograms.size())
					index--;

				std::cout << "[Histogram] Stack [" << i << "]: " << histograms[i][index] << std::endl;

			}
						
		}
		
	}

}

void SpimRegistrationApp::increaseMaxThreshold()
{
	config.threshold.max += 5;
	std::cout << "[Threshold] Max: " << (int)config.threshold.max << std::endl;

	histogramsNeedUpdate = true;
}

void SpimRegistrationApp::increaseMinThreshold()
{
	config.threshold.min += 5;
	if (config.threshold.min > config.threshold.max)
		config.threshold.min = config.threshold.max;
	std::cout << "[Threshold] Min: " << (int)config.threshold.min << std::endl;

	histogramsNeedUpdate = true;
}

void SpimRegistrationApp::decreaseMaxThreshold()
{
	config.threshold.max -= 5;
	if (config.threshold.max < config.threshold.min)
		config.threshold.max = config.threshold.min;

	std::cout << "[Threshold] Max: " << (int)config.threshold.max << std::endl;

	histogramsNeedUpdate = true;
}

void SpimRegistrationApp::decreaseMinThreshold()
{
	config.threshold.min -= 5;
	if (config.threshold.min < 0)
		config.threshold.min = 0;

	std::cout << "[Threshold] Min: " << (int)config.threshold.min << std::endl;

	histogramsNeedUpdate = true;
}


void SpimRegistrationApp::autoThreshold()
{
	using namespace std;

	config.threshold.max = 0;
	config.threshold.min = numeric_limits<unsigned short>::max();
	config.threshold.mean = 0;
	config.threshold.stdDeviation = 0;

	for (size_t i = 0; i < stacks.size(); ++i)
	{
		Threshold t = stacks[i]->getLimits();
		cout << "[Contrast] Stack " << i << " contrast: [" << t.min << " -> " << t.max << "], mean: " << t.mean << ", std dev: " << t.stdDeviation << std::endl;


		config.threshold.min = min(config.threshold.min, t.min);
		config.threshold.max = max(config.threshold.max, t.max);
		config.threshold.mean += t.mean;
		config.threshold.stdDeviation = max(config.threshold.stdDeviation, t.stdDeviation);
	}

	config.threshold.mean /= stacks.size();

	config.threshold.min = (config.threshold.mean - 3 * config.threshold.stdDeviation);
	config.threshold.min = std::max(config.threshold.min, 0.0);

	config.threshold.max = (config.threshold.mean + 3 * config.threshold.stdDeviation);

	cout << "[Contrast] Global contrast: [" << config.threshold.min << " -> " << config.threshold.max << "], mean: " << config.threshold.mean << ", std dev: " << config.threshold.stdDeviation << std::endl;



	histogramsNeedUpdate = true;
}

void SpimRegistrationApp::toggleAllVolumes()
{

	bool stat = !interactionVolumes[0]->enabled;
	for (size_t i = 0; i < interactionVolumes.size(); ++i)
		interactionVolumes[i]->enabled = stat;
}

void SpimRegistrationApp::deselectAll()
{
	currentVolume = -1;
}

void SpimRegistrationApp::subsampleAllStacks()
{
	for (auto it = stacks.begin(); it != stacks.end(); ++it)
		(*it)->subsample(true);
}

void SpimRegistrationApp::calculateHistograms()
{
	histograms.clear();

	size_t maxVal = 0;

	std::cout << "[Contrast] Calculating histograms within " << config.threshold.min << " -> " << config.threshold.max << std::endl;

	for (size_t i = 0; i < stacks.size(); ++i)
	{
		std::vector<size_t> histoRaw = stacks[i]->calculateHistogram(config.threshold);

		for (size_t j = 0; j < histoRaw.size(); ++j)
			maxVal = std::max(maxVal, histoRaw[j]);


		std::cout << "[Debug] Size: " << histoRaw.size() << ", max: " << maxVal << std::endl;
		// convert to floats
		std::vector<float> histoFloat; 
		histoFloat.reserve(histoRaw.size());
		
		for (size_t j = 0; j < histoRaw.size(); ++j)
			histoFloat.push_back((float)histoRaw[j]);

		histograms.push_back(histoFloat);
	}

	std::cout << "[Contrast] Calculated " << histograms.size() << ", normalizing to " << maxVal << " ... \n";
		 

	// normalize based on max histogram value
	for (size_t i = 0; i < histograms.size(); ++i)
	{
		for (size_t j = 0; j < histograms[i].size(); ++j)
		{
			histograms[i][j] /= maxVal;
		}
	}



	histogramsNeedUpdate = false;
}


void SpimRegistrationApp::drawContrastEditor(const Viewport* vp)
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


	const double leftLimit = config.threshold.min;
	const double rightLimit = config.threshold.max;

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
	
	if (histograms.empty())
		return;

	float histoWidth = histograms[0].size();
	// the single offset between histograms
	float delta = vp->size.x / (histoWidth * (histograms.size()+1));

	glBegin(GL_LINES);
	for (size_t i = 0; i < histograms.size(); ++i)
	{
		glm::vec3 rc = getRandomColor((unsigned int)i);
		glColor3fv(glm::value_ptr(rc));
	
		//std::cout << "[Debug] Drawing " << histograms[i].size() << " histogram bins.\n";
			
		for (unsigned int ix = 0; ix < histograms[i].size(); ++ix)
		{
			float x = (float)ix / histograms[i].size();

			// offset each value slightly
			//x += ((float)histograms.size() / (float)vp->size.x);
			x += delta;

			glVertex2f(x, 0.f);
			glVertex2f(x, histograms[i][ix]);
		}



	}
	glEnd();

}


void SpimRegistrationApp::drawBoundingBoxes() const
{
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);

	glActiveTexture(GL_TEXTURE0);
	glDisable(GL_BLEND);

	glEnableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);

	for (size_t i = 0; i < interactionVolumes.size(); ++i)
	{
		if (!interactionVolumes[i]->enabled)
			glColor3f(0.2, 0.2, 0.5);
		else
		{
			glPushMatrix();
			glMultMatrixf(glm::value_ptr(interactionVolumes[i]->getTransform()));

			if ((int)i == currentVolume)
				glColor3f(1, 1, 0);
			else 
				glColor3f(0.6, 0.6, 0.6);
			
			interactionVolumes[i]->getBBox().draw();



			if (interactionVolumes[i]->locked)
			{
				glColor3f(1, 0, 0);
				interactionVolumes[i]->drawLocked();
			}

			glPopMatrix();
		}


	}
	
	glDisableClientState(GL_VERTEX_ARRAY);
	glDepthMask(GL_TRUE);
	glEnable(GL_DEPTH_TEST);
}

void SpimRegistrationApp::drawGroundGrid(const Viewport* vp) const
{
	glColor4f(0.3f, 0.3f, 0.3f, 1.f);

	glPushMatrix();
	glTranslatef(gridOffset.x, gridOffset.y, gridOffset.z);


	if (vp->name == "Perspective" || vp->name == "Ortho Y")
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

	if (vp->name == "Ortho X")
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

	if (vp->name == "Ortho Z")
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

	glPopMatrix();
}

void SpimRegistrationApp::drawViewplaneSlices(const Viewport* vp, const Shader* shader) const
{
	glm::mat4 mvp;// = vp.proj * vp.view;
	vp->camera->getMVP(mvp);
	
	shader->bind();

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

	shader->setUniform("minThreshold", (float)config.threshold.min);
	shader->setUniform("maxThreshold", (float)config.threshold.max);
	shader->setUniform("sliceCount", (float)sliceCount);
	shader->setUniform("stdDev", (float)config.threshold.stdDeviation);

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
		sprintf(uname, "volume[%d].texture", (int)i);
		shader->setUniform(uname, (int)i);

		AABB bbox = reorderedStacks[i]->getBBox();
		sprintf(uname, "volume[%d].bboxMax", (int)i);
		shader->setUniform(uname, bbox.max);
		sprintf(uname, "volume[%d].bboxMin", (int)i);
		shader->setUniform(uname, bbox.min);

		sprintf(uname, "volume[%d].enabled", (int)i);
		shader->setUniform(uname, stacks[i]->enabled);

		sprintf(uname, "volume[%d].inverseMVP", (int)i);
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

void SpimRegistrationApp::drawAxisAlignedSlices(const glm::mat4& mvp, const glm::vec3& viewAxis, const Shader* shader) const
{
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_ONE, GL_ONE); // GL_ONE_MINUS_SRC_ALPHA);
	//glBlendFunc(GL_SRC_ALPHA, GL_ONE);

	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);

	shader->bind();
	shader->setUniform("mvpMatrix", mvp);

	shader->setUniform("maxThreshold", (int)config.threshold.max);
	shader->setUniform("minThreshold", (int)config.threshold.min);

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

void SpimRegistrationApp::drawAxisAlignedSlices(const Viewport* vp, const Shader* shader) const
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

	shader->setUniform("maxThreshold", (float)config.threshold.max);
	shader->setUniform("minThreshold", (float)config.threshold.min);

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



void SpimRegistrationApp::raytraceVolumes(const Viewport* vp) const
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


		sprintf_s(uname, "volume[%d].enabled", i);
		volumeRaycaster->setUniform(uname, stacks[i]->enabled);

#else
		char uname[256];
		sprintf(uname, "volume[%d].texture",(int) i);
		volumeRaycaster->setUniform(uname, (int)i);

		AABB bbox = stacks[i]->getBBox();
		sprintf(uname, "volume[%d].bboxMax", (int)i);
		volumeRaycaster->setUniform(uname, bbox.max);
		sprintf(uname, "volume[%d].bboxMin", (int)i);
		volumeRaycaster->setUniform(uname, bbox.min);

		sprintf(uname, "volume[%d].transform", (int)i);
		volumeRaycaster->setMatrix4(uname, stacks[i]->transform);

		sprintf(uname, "volume[%d].inverseTransform", (int)i);
		volumeRaycaster->setMatrix4(uname, glm::inverse(stacks[i]->transform));
#endif
	}

	// set the global contrast
	volumeRaycaster->setUniform("minThreshold", (float)config.threshold.min);
	volumeRaycaster->setUniform("maxThreshold", (float)config.threshold.max);


	volumeRaycaster->setUniform("activeVolume", (int)currentVolume);

	// bind the ray start/end textures
	volumeRaycaster->setTexture2D("rayStart", rayStartTarget->getColorbuffer(), (int)stacks.size());
	volumeRaycaster->setMatrix4("inverseMVP", imvp);

	volumeRaycaster->setUniform("stepLength", config.raytraceDelta);

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


void SpimRegistrationApp::beginAutoAlign()
{
	if (interactionVolumes.size() < 2 || currentVolume== -1)
		return;

	if (runAlignment)
		return;


	std::cout << "[Debug] Aligning volume" << currentVolume << " to volume 0 ... " << std::endl;
	

	runAlignment = true;      
	solver->initialize(interactionVolumes[currentVolume]);

	saveVolumeTransform(currentVolume);

	multiAlign = false;
	runAlignmentOnlyOncePlease = false;
}

void SpimRegistrationApp::endAutoAlign()
{
	runAlignment = false;
	runAlignmentOnlyOncePlease = false;
	std::cout << "[Debug] Ending auto align.\n";

	selectAndApplyBestSolution();
}

void SpimRegistrationApp::selectAndApplyBestSolution()
{ 
	if (!currentVolumeValid())
		return;

	// apply best transformation
	const IStackTransformationSolver::Solution bestResult = solver->getBestSolution();

	std::cout << "[Debug] Applying best result (id:" << bestResult.id << ", score: " << bestResult.score << ")... \n";
	saveVolumeTransform(currentVolume);
	interactionVolumes[currentVolume]->applyTransform(bestResult.matrix);
	updateGlobalBbox();

}

void SpimRegistrationApp::runAlignmentOnce()
{
	if (interactionVolumes.size() < 2 || currentVolume == -1)
		return;

	if (runAlignment)
		return;


	std::cout << "[Debug] Aligning volume" << currentVolume << " to volume 0 ... " << std::endl;


	runAlignment = true;
	solver->initialize(interactionVolumes[currentVolume]);

	saveVolumeTransform(currentVolume);

	multiAlign = false;
	runAlignmentOnlyOncePlease = true;
}


void SpimRegistrationApp::beginMultiAutoAlign()
{
	if (interactionVolumes.size() < 2 || currentVolume == -1)
		return;

	if (runAlignment)
		return;


	std::cout << "[Debug] Aligning volume" << currentVolume << " to volume 0 ... " << std::endl;
	
	runAlignment = true;
	solver->initialize(interactionVolumes[currentVolume]);

	saveVolumeTransform(currentVolume);

	multiAlign = true;
	runAlignmentOnlyOncePlease = false;
}


void SpimRegistrationApp::undoLastTransform()
{
	if (transformUndoChain.empty())
		return;

	
	VolumeTransform vt = transformUndoChain.back();
	vt.volume->setTransform(vt.matrix);

	transformUndoChain.pop_back();
	updateGlobalBbox();
}

void SpimRegistrationApp::startStackMove(const glm::ivec2& mouse)
{
	if (!currentVolumeValid())
		return;

	//saveStackTransform(currentStack);
	saveVolumeTransform(currentVolume);


	const Viewport* vp = layout->getActiveViewport();
	if (vp && controlWidget)
		controlWidget->beginMouseMove(vp, vp->getRelativeCoords(mouse));
	


}

void SpimRegistrationApp::updateStackMove(const glm::ivec2& mouse)
{
	if (!currentVolumeValid())
		return;

	updateGlobalBbox();

	const Viewport* vp = layout->getActiveViewport();
	if (vp && controlWidget)
		controlWidget->updateMouseMove(vp, vp->getRelativeCoords(mouse));

}

void SpimRegistrationApp::updateStackMove(const glm::ivec2& mouse, float valueStep)
{
	if (!currentVolumeValid())
		return;

	updateGlobalBbox();

	const Viewport* vp = layout->getActiveViewport();
	if (vp && controlWidget)
		controlWidget->updateMouseMove(vp, vp->getRelativeCoords(mouse), valueStep);
}




void SpimRegistrationApp::endStackMove(const glm::ivec2& mouse)
{

	if (!currentVolumeValid())
		return;

	updateGlobalBbox();


	const Viewport* vp = layout->getActiveViewport();
	if (vp && controlWidget)
		controlWidget->endMouseMove(vp, vp->getRelativeCoords(mouse));


}

void SpimRegistrationApp::saveVolumeTransform(unsigned int n)
{
	assert(n < interactionVolumes.size());

	VolumeTransform vt;
	vt.matrix = interactionVolumes[n]->getTransform();
	vt.volume = interactionVolumes[n];

	transformUndoChain.push_back(vt);
}

void SpimRegistrationApp::updateGlobalBbox()
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

void SpimRegistrationApp::centerGrid()
{
	gridOffset = globalBBox.getCentroid();
}


void SpimRegistrationApp::update(float dt)
{
	using namespace std;

	if (runAlignment)
	{
		if (solver->nextSolution())
		{
			std::cout << "[Align] Testing transform " << solver->getCurrentSolution().id << " ... \n";
		}
		else
		{
			std::cout << "[Aling] No more transformations to test!\n";
		
		
			if (multiAlign)
			{
				selectAndApplyBestSolution();

				currentVolume = rand() % interactionVolumes.size();

				// select better solver


				solver->initialize(interactionVolumes[currentVolume]);

			}
		
			if (runAlignmentOnlyOncePlease)
				endAutoAlign();

		}
	}


	static float time = 2.f;
	time -= dt;

	if (cameraAutoRotate)
	{
		float rot = dt * 5.f;
		rotateCamera(glm::vec2(0.f, rot));


	}
	
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
	{

		addMultiStackSamples();
		//addStackSamples();
	}
}

void SpimRegistrationApp::maximizeViews()
{
	if (currentVolumeValid())
	{
		for (auto it = prevLayouts.begin(); it != prevLayouts.end(); ++it)
			it->second->maximizeView(interactionVolumes[currentVolume]->getTransformedBBox());

	}
	else
	{
		for (auto it = prevLayouts.begin(); it != prevLayouts.end(); ++it)
			it->second->maximizeView(globalBBox);
	}
	
	
}

void SpimRegistrationApp::toggleSlices()
{
	drawSlices = !drawSlices;
	std::cout << "[Render] Drawing " << (drawSlices ? "slices" : "volumes") << std::endl;
}

void SpimRegistrationApp::inspectOutputImage(const glm::ivec2& cursor)
{
	using namespace glm;

	// only work with fullscreen layouts for now 
	if (!layout->isSingleView())
		return;

	if (currentVolumeValid())
		return;

	if (controlWidget)
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

void SpimRegistrationApp::increaseSliceCount()
{
	sliceCount = (unsigned int)(sliceCount * 1.4f);
	sliceCount = std::min(sliceCount, MAX_SLICE_COUNT);
	std::cout << "[Slices] Slicecount: " << sliceCount << std::endl;
}


void SpimRegistrationApp::decreaseSliceCount()
{
	sliceCount = (unsigned int)(sliceCount / 1.4f);
	sliceCount = std::max(sliceCount, MIN_SLICE_COUNT);
	std::cout << "[Slices] Slicecount: " << sliceCount << std::endl;
}

void SpimRegistrationApp::resetSliceCount()
{
	sliceCount = STD_SLICE_COUNT;
	std::cout << "[Slices] Slicecount: " << sliceCount << std::endl;
}


void SpimRegistrationApp::calculateImageContrast(const std::vector<glm::vec4>& img)
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

void SpimRegistrationApp::drawPointclouds(const Viewport* vp)
{
	glm::mat4 mvp(1.f);
	vp->camera->getMVP(mvp);
	
	for (size_t i = 0; i < pointclouds.size(); ++i)
	{
		pointclouds[i]->draw();
	}
}


void SpimRegistrationApp::drawPointSpritePointclouds(const Viewport* vp)
{
	glm::mat4 mvp(1.f);
	vp->camera->getMVP(mvp);

	glPointSize(5);
	glEnable(GL_POINT_SPRITE);


	pointSpriteShader->bind();
	pointSpriteShader->setUniform("mvp", mvp);
	pointSpriteShader->setTexture2D("gaussmap", pointSpriteTexture);

	glEnableClientState(GL_VERTEX_ARRAY);

	
	for (size_t i = 0; i < pointclouds.size(); ++i)
	{
		pointSpriteShader->setUniform("transform", pointclouds[i]->getTransform());
		pointclouds[i]->draw();
	}
		
	glDisableClientState(GL_VERTEX_ARRAY);

	pointSpriteShader->disable();
	glDisable(GL_POINT_SPRITE);
	glPointSize(1);

}


void SpimRegistrationApp::drawSolutionParameterSpace(const Viewport* vp) const
{
	using namespace glm;

	if (solutionParameterSpace.empty())
		return;
	


	const vec3 red(0.8, 0, 0);
	const vec3 grn(0, 0.7, 0);

	glBegin(GL_POINTS);
	for (size_t i = 0; i < solutionParameterSpace.size(); ++i)
	{
		glm::vec3 color = mix(red, grn, solutionParameterSpace[i].a);
		glVertex3fv(value_ptr(solutionParameterSpace[i]));
	}

	glEnd();

}

void SpimRegistrationApp::drawRays(const Viewport* vp)
{
	glColor3f(1, 0, 0);
	glLineWidth(2.f);
	glBegin(GL_LINES);
	for (size_t i = 0; i < rays.size(); ++i)
	{
		glVertex3fv(glm::value_ptr(rays[i].origin));
		glVertex3fv(glm::value_ptr(rays[i].direction));

	}
	glEnd();
	glLineWidth(1.f);
}

void SpimRegistrationApp::inspectPointclouds(const Ray& r)
{
	for (size_t i = 0; i < pointclouds.size(); ++i)
	{
		const SimplePointcloud* spc = pointclouds[i];
		if (r.intersectsAABB(spc->getTransformedBBox()))
		{

			float dist = -1.f;
			size_t hit = r.getClosestPoint(spc->getPoints(), spc->getTransform(), dist);

			std::cout << "[Debug] Ray intersects point cloud " << i << " at point index: " <<hit  << ", dist: " << sqrtf(dist) << std::endl;


		}


	}
}

void SpimRegistrationApp::drawScoreHistory(const TinyHistory<double>& hist) const
{
	if (!drawHistory)
		return;

	
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

void SpimRegistrationApp::selectSolver(const std::string& name)
{
	// do not change solvers mid-run
	if (runAlignment)
		return;


	using namespace std;
	IStackTransformationSolver* newSolver = nullptr;

	if (name == "Uniform DX")
	{
		cout << "[Solver] Creating new Uniform DX Solver\n";
		newSolver = new DXSolver;
	}

	if (name == "Uniform DY")
	{
		cout << "[Solver] Creating new Uniform DY Solver\n";
		newSolver = new DYSolver;
	}

	if (name == "Uniform DZ")
	{
		cout << "[Solver] Creating new Uniform DZ Solver\n";
		newSolver = new DZSolver;
	}

	if (name == "Uniform RY")
	{
		cout << "[Solver] Creating new Uniform RY Solver\n";
		newSolver = new RYSolver;
	}

	if (name == "Simulated Annealing")
	{
		cout << "[Solver] Creating new Simulated Annealing Solver\n";
		newSolver = new SimulatedAnnealingSolver;
	}

	if (name == "Hillclimb")
	{
		cout << "[Solver] Creating new Multidimensional hillclimb solver\n";

		std::vector<InteractionVolume*> volumes;
		for (size_t i = 0; i < stacks.size(); ++i)
			volumes.push_back(stacks[i]);
		newSolver = new MultiDimensionalHillClimb(volumes);
	}

	if (name == "Solution Parameterspace")
	{
		cout << "[Solver] Creating new solution parameter space explorer.\n";
		newSolver = new ParameterSpaceMapping;
	}

	if (name == "Random Rotation")
	{
		cout << "[Solver] Creating new random rotation solver\n";
		newSolver = new RandomRotationSolver;
	}

	if (name == "Uniform Scale")
	{
		cout << "[Solver] Creating new uniform scale solver\n";
		newSolver = new UniformScaleSolver;
	}


	// only switch solvers if we have created a valid one
	if (newSolver)
	{
		delete solver;
		solver = newSolver;

		cout << "[Debug] Switched solvers.\n";
	
	}
}

void SpimRegistrationApp::toggleHistory()
{
	drawHistory = !drawHistory;
	std::cout << "[Debug] " << (drawHistory ? "D" : "Not d") << "rawing score history.\n";
}

void SpimRegistrationApp::clearHistory()
{
	if (solver)
		solver->clearHistory();

	scoreHistory.history.clear();
	scoreHistory.reset();
}

double SpimRegistrationApp::calculateImageScore()
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


void SpimRegistrationApp::readbackRenderTarget()
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

void SpimRegistrationApp::initializeRayTargets(const Viewport* vp)
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

void SpimRegistrationApp::drawPhantomBoxes() const
{
	if (phantoms.empty())
		return;


	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);

	glActiveTexture(GL_TEXTURE0);
	glDisable(GL_BLEND);

	glEnableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);


	for (size_t i = 0; i < phantoms.size(); ++i)
	{
		glPushMatrix();
		glMultMatrixf(glm::value_ptr(phantoms[i].transform));


		if ((int)i == currentVolume)
			glColor3f(0.8, 0.5, 0);
		else
			glColor3f(0.5f, 0.0f, 0.0f);
		
		phantoms[i].bbox.draw();

		glPopMatrix();
		
	}

	glDisableClientState(GL_VERTEX_ARRAY);
	glDepthMask(GL_TRUE);
	glEnable(GL_DEPTH_TEST);
}

static glm::mat4 loadRegistration(const std::string& filename)
{

	std::ifstream file(filename);

	if (!file.is_open())
		throw std::runtime_error("Unable to open file \"" + filename + "\"");

	glm::mat4 T(1.f);


	float* m = glm::value_ptr(T);

	for (int i = 0; i < 16; ++i)
		file >> m[i];

	return T;
}


static glm::mat4 loadFijiRegistration(const std::string& filename)
{
	using namespace std;
	
	ifstream file(filename);

	if (!file.is_open())
		throw std::runtime_error("Unable to open file \"" + filename + "\"");

	glm::mat4 T(1.f);
	float* m = glm::value_ptr(T);

	for (int i = 0; i < 16; ++i)
	{
		string buffer;
		getline(file, buffer);

		int result = sscanf_s(buffer.c_str(), "m%*2s: %f", &m[i]);
		assert(result == 1);
	}


	return glm::transpose(T);
}

void SpimRegistrationApp::addPhantom(const std::string& stackFilename, const std::string& referenceTransform, const glm::vec3& voxelDimensions, bool fijiTransform)
{
	Phantom p;
	
	p.stackFile = stackFilename;

	// load the transformation
	if (fijiTransform)
		p.transform = loadFijiRegistration(referenceTransform);
	else
		p.transform = loadRegistration(referenceTransform);

	p.originalTransform = p.transform;

	bool loadedBbox = false;
	// first see if we loaded the stack already
	for (size_t i = 0; i < stacks.size(); ++i)
	{
		if (stacks[i]->getFilename() == stackFilename)
		{
			std::cout << "[Debug] Found stack \"" << stackFilename << "\" in loaded stacks, reusing ...\n";
			p.bbox = stacks[i]->getBBox();

			loadedBbox = true;
			break;
		}
	}

	if (!loadedBbox)
	{
		std::cout << "[Phantom] Loading stack \"" << stackFilename << "\" for dimensions.\n";
		std::auto_ptr<SpimStack> s(SpimStack::load(stackFilename));
		s->setVoxelDimensions(voxelDimensions);
		p.bbox = s->getBBox();
	}
	
	phantoms.push_back(p);
}


void SpimRegistrationApp::alignPhantoms()
{
	if (phantoms.size() != stacks.size())
	{
		std::cerr << "[Phantom] Different number of stacks and phantoms, aborting.\n";
		return;
	}

	std::cout << "[Phantom] Aligning phantoms. Assuming a 1:1 match btw phantoms and stacks!\n";


	TinyStats<double> allError;

	// do it for all combinations to average out error, as one transform will always map perfectly
	for (size_t k = 0; k < stacks.size(); ++k)
	{

		// calculate offset matrix here ...
		glm::mat4 offset(1.f);
		offset = stacks[k]->getTransform() * glm::inverse(phantoms[k].originalTransform);

		// apply offset matrix
		for (size_t i = 0; i < phantoms.size(); ++i)
			phantoms[i].transform = offset * phantoms[i].originalTransform;


		TinyStats<double> error;
		// calculate difference/distance here
		for (size_t i = 0; i < phantoms.size(); ++i)
		{
			std::vector<glm::vec3> verts = phantoms[i].bbox.getVertices();

			double err = 0;
			for (int j = 0; j < 8; ++j)
			{
				glm::vec4 v(verts[j], 1.f);
				glm::vec4 r = stacks[i]->getTransform() * v;
				glm::vec4 s = phantoms[i].transform * v;

				err += glm::distance(r, s);
			}

			err /= 8;
			error.add(err);

			allError.add(err);
		}

		std::cout << "[Phantom] Base [" << k << "] mean phantom distance error: " << error.getMean() << std::endl;
	}

	std::cout << "[Phantom] Overall mean phantom distance error: " << allError.getMean() << std::endl;

}

/*
void SpimRegistrationApp::loadPrevSolutionSpace(const std::string& filename)
{
	std::ifstream file(filename);
	assert(file.is_open());

	solutionParameterSpace.clear();


	std::cout << "[Debug] Reading lines from \"" << filename << "\" ... ";

	// ignore first line
	std::string temp;
	std::getline(file, temp);

	while (!file.eof())
	{

		std::getline(file, temp);
		
		glm::vec4 s(0);
		sscanf_s(temp.c_str(), "%f,%f,%f", &s.x, &s.z, &s.a);

		solutionParameterSpace.push_back(s);
	}

	std::cout << "done.\n";

	// normalize
	float maxVal = std::numeric_limits<float>::lowest();
	float minVal = std::numeric_limits<float>::max();


	for (size_t i = 0; i < solutionParameterSpace.size(); ++i)
	{
		maxVal = std::max(maxVal, solutionParameterSpace[i].a);
		minVal = std::min(minVal, solutionParameterSpace[i].a);
	}

	for (size_t i = 0; i < solutionParameterSpace.size(); ++i)
	{
		solutionParameterSpace[i].a -= minVal;
		solutionParameterSpace[i].a /= (maxVal - minVal);

	}

	std::cout << "[Debug] Loaded solution parameterspace of " << solutionParameterSpace.size() << " values.\n";


}
*/


void SpimRegistrationApp::createFakeBeads(unsigned int beadCount)
{
	using namespace std;
	using namespace glm;

	struct BeadInfo
	{
		vec3		L;
		vec3		W;
		float		weight;

		int			descriptorCorrelation;
		int			ransacCorrelation;
	};


	map<unsigned int, vector<BeadInfo>> beadInfo;

	for (size_t i = 0; i < stacks.size(); ++i)
	{
		vector<BeadInfo> beads;
		beads.reserve(beadCount / 2);
		beadInfo[i] = move(beads);
	}


	mt19937 rng(time(0));

	vector<bool> visible(stacks.size(), false);
	solutionParameterSpace.clear();


	int descriptorCorrelationCounter = 0;

	cout << "[Beads] Creating " << beadCount << " fake beads ... ";

	for (unsigned int n = 0; n < beadCount; ++n)
	{


		// create random sample
		vec3 pos = globalBBox.min + globalBBox.getSpan() * vec3((float)rng() / rng.max(), (float)rng() / rng.max(), (float)rng()/rng.max());
		
		int visibleCount = 0;
		for (size_t i = 0; i < stacks.size(); ++i)
		{
			visible[i] = stacks[i]->isInsideVolume(pos);
			if (visible[i])
				++visibleCount;
		}


		// update debug drawing
		vec4 sol(pos, (float)visibleCount / stacks.size());
		solutionParameterSpace.push_back(sol);
		
		

		for (size_t i = 0; i < stacks.size(); ++i)
		{
			BeadInfo info;

			// ???
			info.weight = 1.0f;

			info.descriptorCorrelation = n;

			// ?????
			info.ransacCorrelation = 0;


			if (visible[i])
			{
				vec4 stackCoords = stacks[i]->getInverseTransform() * vec4(pos, 1.f);
				
				info.L = vec3(stackCoords);
				info.W = info.L;

				beadInfo[i].push_back(info);
			}

		}
	}

	cout << "done.\n";


	for (size_t i = 0; i< stacks.size(); ++i)
	{
		string path = stacks[i]->getFilename().substr(0, stacks[i]->getFilename().find_last_of("/"));
		string file = stacks[i]->getFilename().substr(stacks[i]->getFilename().find_last_of("/") + 1);

		path += "/registration/";

		string filename = path + file;

		cout << "[Beads] Saving file \"" << filename << "\"\n";

		ofstream beadsFile(filename + ".beads.txt");
		if (!beadsFile.is_open())
			throw(runtime_error("Unable to open file \"" + filename + "\" for writing.\n"));

		beadsFile << "ID\tViewID\t\tLx\tLy\tLz\tWx\tWy\tWz\tWeight\tDescCorr\tRansacCorr\n";
		
		for (int k = 0; k < beadInfo[i].size(); ++k)
		{
			const BeadInfo& b = beadInfo[i][k];
			beadsFile << k << "\t" << i << "\t" << b.L.x << "\t" << b.L.y << "\t" << b.L.z << "\t" << b.W.x << "\t" << b.W.y << "\t" << b.W.z << "\t" << b.weight << "\t" << b.descriptorCorrelation << ":" << i << "\t" << b.ransacCorrelation << endl;
		}




		ofstream dimFile(filename + ".dim");
		if (dimFile.is_open())
			throw(runtime_error("Unable to open file \"" + filename + "\" for writing."));
		dimFile << "image width: " << stacks[i]->getWidth() << endl;
		dimFile << "image height: " << stacks[i]->getHeight() << endl;
		dimFile << "image depth: " << stacks[i]->getDepth() << endl;




		ofstream regoFileOut(filename + ".registration");
		if (regoFileOut.is_open())
			throw(runtime_error("Unable to open file \"" + filename + "\" for writing."));
		
		const mat4& m = stacks[i]->getTransform();
		for (int i = 0; i < 4; ++i)
			for (int k = 0; k < 4; ++k)
			{
				regoFileOut << "m" << i << k << ": " << m[i][k] << endl;
			}

		regoFileOut << "AffineModel3D\n";
		regoFileOut << endl;

		// note this should all be saved from prev runs!
		regoFileOut << "minError: 0\n";
		regoFileOut << "avgError: 0\n";
		regoFileOut << "maxError: 0\n";
		regoFileOut << endl;

		float zscaling = 1.f;
		regoFileOut << "z - scaling : " << zscaling << endl;


		regoFileOut << "Angle Specific Average Error : 0.0\n";
		regoFileOut << "Overlapping Views : " << stacks.size() << endl;;

		regoFileOut << "Num beads having true correspondences : 0\n";
		regoFileOut << "Sum of true correspondences pairs : 0\n";
		regoFileOut << "Num beads having correspondences candidates : 0\n";
		regoFileOut << "Sum of correspondences candidates pairs : 0\n";
		regoFileOut << endl;



		for (size_t k = 0; k < stacks.size(); ++k)
		{
			if (i != k)
			{
				regoFileOut << stacks[k]->getFilename() << " - Average Error: 0\n";
				regoFileOut << stacks[k]->getFilename() << " - Bead Correspondences: 0\n";
				regoFileOut << stacks[k]->getFilename() << " - Ransac Correspondences: 0\n";
				regoFileOut << endl;
			}
		}




	}


	cout << "done.\n";

}

void SpimRegistrationApp::setWidgetType(const std::string& t)
{
	if (t == "None")
	{
		std::cout << "[Control] Disabling current translation widget.\n";
		delete controlWidget;
		controlWidget = nullptr;

	}
	else if (t == "Translate")
	{		
		// toggle -- if we already are in translate m
		if (controlWidget && dynamic_cast<TranslateWidget*>(controlWidget))
		{

			std::cout << "[Control] Disabling current translation widget.\n";
			delete controlWidget;
			controlWidget = nullptr;
		}
		else
		{
			std::cout << "[Control] Creating new translation widget.\n";
			delete controlWidget;
			controlWidget = new TranslateWidget;
		}
	}
	
	else if (t == "Rotate")
	{

		if (controlWidget && dynamic_cast<RotateWidget*>(controlWidget))
		{
			std::cout << "[Control] Disabling current rotation widget.\n";
			delete controlWidget;
			controlWidget = nullptr;
		}
		else
		{
			std::cout << "[Control] Creating new rotation widget.\n";
			delete controlWidget;
			controlWidget = new RotateWidget;
		}
	}
	else if (t == "Scale")
	{
		if (controlWidget && dynamic_cast<ScaleWidget*>(controlWidget))
		{
			std::cout << "[Control] Disabling current scale widget.\n";
			delete controlWidget;
			controlWidget = nullptr;
		}
		else
		{
			std::cout << "[Control] Creating new scale widget.\n";
			delete controlWidget;
			controlWidget = new ScaleWidget;
		}

	}

	if (controlWidget)
		if (currentVolumeValid())
			controlWidget->setInteractionVolume(interactionVolumes[currentVolume]);
		else
			controlWidget->setInteractionVolume(nullptr);
	
}

void SpimRegistrationApp::setWidgetMode(const std::string& mode)
{
	if (controlWidget)
		controlWidget->setMode(mode);
}

void SpimRegistrationApp::createEmptyFullSizedStack()
{
	assert(!stacks.empty());
	using namespace glm;
	
	// calculate resolution based on global bbox
	const ivec3 res(ceil((globalBBox.getSpan()) / config.defaultVoxelSize));

	std::cout << "[Resample] Creating empty stack with res of " << res << " and voxel dimensions of " << config.defaultVoxelSize<< std::endl;
	
	SpimStackU16* stack = new SpimStackU16;
	stack->setVoxelDimensions(config.defaultVoxelSize);
	stack->setContent(res, 0);
	

	stacks.push_back(stack);
	addInteractionVolume(stack);
	saveVolumeTransform(stacks.size() - 1);
	
	// center the stack?
	mat4 T = translate(globalBBox.min);
	stack->setTransform(T);
	
	updateGlobalBbox();
	reloadVolumeShader();
}

void SpimRegistrationApp::createEmptyRandomStack(const glm::ivec3& resolution, const glm::vec3& voxelDimensions)
{
	assert(!stacks.empty());
	using namespace glm;

	std::cout << "[Resample] Creating empty stack with res of " << resolution << " and voxel dimensions of " << voxelDimensions << std::endl;

	auto seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
	auto rand = std::bind(std::uniform_real_distribution<float>(-1.f, 1.f), std::mt19937(seed));

	
	SpimStackU16* stack = new SpimStackU16;
	stack->setVoxelDimensions(voxelDimensions);
	stack->setContent(resolution, 0);

	stacks.push_back(stack);
	addInteractionVolume(stack);
	saveVolumeTransform(stacks.size() - 1);

	/*
	// copy the transform of the base stack
	stack->setTransform(stacks[0]->getTransform());

	// create random translation
	vec3 delta(rand(), rand(), rand());

	delta *= vec3(1000, 200, 1000);
	stack->move(delta);


	// create random rotation
	float a = rand() * 90.f - 45.f;
	stack->setRotation(a);

	std::cout << "[Resample] Created random stack with dT " << delta << " and R=" << a << std::endl;
	*/

	updateGlobalBbox();
	reloadVolumeShader();
}

void SpimRegistrationApp::reloadConfig()
{
	config.reload(); 
	reloadVolumeShader(); 

	// apply new default scale to all volumes?
	for (size_t i = 0; i < stacks.size(); ++i)
	{
		stacks[i]->setVoxelDimensions(config.defaultVoxelSize);
		stacks[i]->update();
	}

	updateGlobalBbox();
	
}


void SpimRegistrationApp::drawStackSamples() const
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

void SpimRegistrationApp::addStackSamples()
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
	std::vector<float> samples(sliceSamples.size());
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


void SpimRegistrationApp::addMultiStackSamples()
{
	if (sampleStack == -1)
		return;



	SpimStack* stack = stacks[sampleStack];

	if (lastStackSample >= stack->getDepth() || lastStackSample < 0)
		return;

	//std::cout << "[Sampling] Rendering slice " << lastStackSample << " ... \n";

	stackSamplerTarget->bind();
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	gpuMultiStackSampler->bind();

	// set sampling volume
	gpuMultiStackSampler->setUniform("planeTransform", stack->getTransform());
	gpuMultiStackSampler->setUniform("zPlane", (float)lastStackSample);
	gpuMultiStackSampler->setUniform("planeResolution", (float)stack->getWidth(), (float)stack->getHeight());
	gpuMultiStackSampler->setUniform("planeScale", stack->getVoxelDimensions());

	// set reference volumes
	for (size_t i = 0; i < stacks.size(); ++i)
	{
		const std::string volumeName = std::string("volumes[") + std::to_string(i) + std::string("]");
	
		gpuMultiStackSampler->setTexture3D(volumeName + ".volume", stacks[i]->getTexture(), i);		
		gpuMultiStackSampler->setUniform(volumeName + ".inverseTransform", stacks[i]->getInverseTransform());
		gpuMultiStackSampler->setUniform(volumeName + ".resolution", stacks[i]->getVoxelDimensions());
		gpuMultiStackSampler->setUniform(volumeName + ".scale", stacks[i]->getWidth(), stacks[i]->getHeight(), stacks[i]->getDepth());
		gpuMultiStackSampler->setUniform(volumeName + ".enabled", i != sampleStack);
	}


	// draw the correct z-plane
	glBegin(GL_QUADS);
	glVertex2i(0, 0);
	glVertex2i(1, 0);
	glVertex2i(1, 1);
	glVertex2i(0, 1);
	glEnd();


	gpuMultiStackSampler->disable();
	stackSamplerTarget->disable();


	// read back
	stackSamplerTarget->bind();
	std::vector<glm::vec4> sliceSamples(stack->getWidth()*stack->getHeight());
	glReadBuffer(GL_COLOR_ATTACHMENT0);

	glReadPixels(0, 0, stackSamplerTarget->getWidth(), stackSamplerTarget->getHeight(), GL_RGBA, GL_FLOAT, glm::value_ptr(sliceSamples[0]));

	stackSamplerTarget->disable();
	glReadBuffer(GL_BACK);



	// set the sample
	std::vector<float> samples(sliceSamples.size());
	for (size_t i = 0; i < samples.size(); ++i)
		samples[i] = sliceSamples[i].a;

	stack->setPlaneSamples(samples, lastStackSample);

	stackSamples = sliceSamples;

	if (lastStackSample % 10 == 0)
		std::cout << "[Sample] Read back " << lastStackSample << "/" << stack->getDepth() << std::endl;

	++lastStackSample;
	if (lastStackSample == stack->getDepth())
	{
		endSampleStack();
		stackSamples.clear();
	}

}


void SpimRegistrationApp::startSampleStack(int n)
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

void SpimRegistrationApp::endSampleStack()
{

	// update the texture here?



	// save the created stack
	
	// determine the save path from stack[0] -- the stack we create our phantoms from
	const std::string baseFile = stacks[0]->getFilename();
	const std::string savePath = baseFile.substr(0, baseFile.find_last_of("/")+1);
	
	std::cout << "[Debug] Save path: " << savePath << std::endl;

	std::string filename = savePath + "phantom_" + std::to_string(sampleStack);;
	stacks[sampleStack]->save(filename + ".tiff");
	stacks[sampleStack]->saveTransform(filename + ".tiff.reference.txt");


	// create a phantom as well
	Phantom p;
	p.bbox = stacks[sampleStack]->getBBox();
	p.originalTransform = stacks[sampleStack]->getTransform();
	p.transform = p.originalTransform;
	p.stackFile = stacks[sampleStack]->getFilename();
	phantoms.push_back(p);


	sampleStack = -1;
	lastStackSample = 0;
	

}

void SpimRegistrationApp::clearSampleStack()
{
	stackSamples.clear();
	lastStackSample = 0;
}


void SpimRegistrationApp::bakeSelectedTransform()
{
	if (currentVolume == -1)
		return;

	if (pointclouds.empty())
	{
		std::cout << "[Error] Unable to bake transform, no valid pointclouds\n";
		return;
	}
		
	if (currentVolume < pointclouds.size())
		pointclouds[currentVolume]->bakeTransform();

}

void SpimRegistrationApp::saveCurrentPointcloud()
{
	if (currentVolume == -1)
		return;

	if (pointclouds.empty())
	{
		std::cout << "[Error] Unable to save, no valid pointclouds\n";
		return;
	}


	if (currentVolume < pointclouds.size())
	{

		std::string filename(pointclouds[currentVolume]->getFilename() + "_out.bin");
		pointclouds[currentVolume]->saveBin(filename);
	}

}

void SpimRegistrationApp::createPointSpriteTexture()
{
	glGenTextures(1, &pointSpriteTexture);
	glBindTexture(GL_TEXTURE_2D, pointSpriteTexture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	const size_t RES = 32;
	std::vector<float> pixels(RES*RES);

	// create a gaussian blur
	for (size_t x = 0; x < RES; ++x)
	{
		for (size_t y = 0; y < RES; ++y)
		{
			// normalize and transform to center
			float X = (float)x / RES;
			X *= 2;
			X -= 1;

			float Y = (float)y / RES;
			Y *= 2;
			Y -= 1;


			float r = sqrtf(X*X + Y*Y);
			float v = exp(-4 * r);

			pixels[x + y*RES] = v;
		}
	}

	/*
	std::cout << "[Debug] Created gauss texture:\n";
	for (size_t y = 0; y < RES; ++y)
	{
		for (size_t x = 0; x < RES; ++x)
			std::cout << std::setprecision(3) << pixels[x + y*RES] << " ";
		std::cout << std::endl;
	}
	*/


	glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, RES, RES, 0, GL_R32F, GL_FLOAT, &pixels[0]);


}

void SpimRegistrationApp::resliceStack(unsigned int stack)
{
	if (stack > stacks.size())
	{
		std::cerr << "[Reslice] Stack " << stack << " outside valid range.\n";
		return;
	}

	stacks[stack]->reslice(0, stacks[stack]->getDepth() / 2);
}


