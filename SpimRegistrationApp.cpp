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

SpimRegistrationApp::SpimRegistrationApp(const glm::ivec2& res) : pointShader(nullptr), sliceShader(nullptr), volumeShader(nullptr),
	volumeRaycaster(nullptr), drawQuad(nullptr), volumeDifferenceShader(nullptr), tonemapper(nullptr), layout(nullptr),
	drawGrid(true), drawBboxes(false), drawSlices(false), drawRegistrationPoints(false), sliceCount(100),
	configPath("./"), cameraMoving(false), runAlignment(false), histogramsNeedUpdate(false), minCursor(0.f), maxCursor(1.f),
	subsampleOnCameraMove(false), useOcclusionQuery(false),
	useImageAutoContrast(false), currentVolume(-1), solver(nullptr)
{
	globalBBox.reset();
	layout = new PerspectiveFullLayout(res);

	prevLayouts["Perspective"] = layout;

	globalThreshold.min = 100;
	globalThreshold.max = 130; // std::numeric_limits<unsigned short>::max();

	resetSliceCount();
	reloadShaders();

	volumeRenderTarget = new Framebuffer(512, 512, GL_RGBA32F, GL_FLOAT);

	glGenQueries(1, &singleOcclusionQuery);
		
	useOcclusionQuery = false;
	calculateScore = true;
	
	solver = new RYSolver;
	//solver = new SimulatedAnnealingSolver;
}

SpimRegistrationApp::~SpimRegistrationApp()
{
	delete solver;
	
	delete volumeRenderTarget;
	
	delete drawQuad;
	delete pointShader;
	delete sliceShader;
	delete volumeShader;
	delete volumeDifferenceShader;
	delete volumeRaycaster;


	glDeleteQueries(1, &singleOcclusionQuery);


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
	
	int numberOfVolumes = std::max(1, (int)stacks.size());
	std::vector<std::string> defines;
	defines.push_back("#define VOLUMES " + boost::lexical_cast<std::string>(numberOfVolumes) + "\n");

	bool enableShaderPreProcessing = false;

	delete pointShader;
	pointShader = new Shader("shaders/points2.vert", "shaders/points2.frag");

	delete sliceShader;
	sliceShader = new Shader("shaders/slices.vert", "shaders/slices.frag");

	delete volumeShader;
	if (enableShaderPreProcessing)
		volumeShader = new Shader("shaders/volume2.vert", "shaders/volume2.frag", defines);
	else
		volumeShader = new Shader("shaders/volume2.vert", "shaders/volume2.frag");

	delete volumeRaycaster;
	volumeRaycaster = new Shader("shaders/volumeRaycast.vert", "shaders/volumeRaycast.frag");

	delete drawQuad;
	drawQuad = new Shader("shaders/drawQuad.vert", "shaders/drawQuad.frag");

	delete volumeDifferenceShader;
	if (enableShaderPreProcessing )
		volumeDifferenceShader = new Shader("shaders/volumeDist.vert", "shaders/volumeDist.frag", defines);
	else
		volumeDifferenceShader = new Shader("shaders/volumeDist.vert", "shaders/volumeDist.frag");

	delete tonemapper;
	if (enableShaderPreProcessing)
		tonemapper = new Shader("shaders/drawQuad.vert", "shaders/tonemapper.frag", defines);
	else
		tonemapper = new Shader("shaders/drawQuad.vert", "shaders/tonemapper.frag");

}

void SpimRegistrationApp::draw()
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

			// apply new transform
			if (runAlignment)
			{

				try
				{

					int query = vp->name;
					assert(query < 4);

					const glm::mat4& mat = solver->getCurrentSolution().matrix;

					saveVolumeTransform(currentVolume);
					interactionVolumes[currentVolume]->applyTransform(mat);

				}
				catch (std::runtime_error& e)
				{
					std::cerr << "[Error] " << e.what() << std::endl;
				}
			}


			if ((runAlignment || calculateScore) && useOcclusionQuery)
			{
				glBeginQuery(GL_SAMPLES_PASSED, singleOcclusionQuery);
				//glBeginQuery(GL_SAMPLES_PASSED, occlusionQueries[query]);
			}

			/// actual drawing block begins
			/// --------------------------------------------------------

			volumeRenderTarget->bind();
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

				
			glEnable(GL_BLEND);
			glBlendEquation(GL_FUNC_ADD);

			//glBlendEquation(GL_MAX);


			glBlendFunc(GL_SRC_ALPHA, GL_ONE);
			glBlendFunc(GL_ONE, GL_ONE);

			// align three-color view
			drawViewplaneSlices(vp, volumeDifferenceShader);
		
			// normal view
			//drawViewplaneSlices(vp, volumeShader);

			
			glDisable(GL_BLEND);
			

			/*
			if (!pointclouds.empty())
				drawPointclouds(vp);
			*/

			//drawRays(vp);


			
			/// --------------------------------------------------------
			/// drawing block ends



			if ((runAlignment || calculateScore) && useOcclusionQuery)
			{
				glEndQuery(GL_SAMPLES_PASSED);
			
			}

			glDisable(GL_BLEND);
			glBlendEquation(GL_FUNC_ADD);
			
			volumeRenderTarget->disable();
			
			
			
			
			drawTonemappedQuad();
			//drawTexturedQuad(volumeRenderTarget->getColorbuffer());


			if (drawGrid)
				drawGroundGrid(vp);
			if (drawBboxes)
				drawBoundingBoxes();


			// the query result should be done by now
			if (runAlignment)
			{
				undoLastTransform();

				if (useOcclusionQuery)
				{
					GLint queryStatus = GL_FALSE;
					while (queryStatus == GL_FALSE)
						glGetQueryObjectiv(singleOcclusionQuery, GL_QUERY_RESULT_AVAILABLE, &queryStatus);

					GLuint64 result = 0;
					glGetQueryObjectui64v(singleOcclusionQuery, GL_QUERY_RESULT, &result);
				
					double relativeResult = (double)result / (double)(volumeRenderTarget->getWidth()*volumeRenderTarget->getHeight());

					solver->recordCurrentScore(relativeResult);
				}
				else
				{
					// image-based metric
					solver->recordCurrentScore(volumeRenderTarget);

				}
			}
			else
			{
				// only calculate score if the solver has not alreay
				if (calculateScore)
				{
					if (useOcclusionQuery)
					{
						GLint queryStatus = GL_FALSE;
						while (queryStatus == GL_FALSE)
							glGetQueryObjectiv(singleOcclusionQuery, GL_QUERY_RESULT_AVAILABLE, &queryStatus);

						GLuint64 result = 0;
						glGetQueryObjectui64v(singleOcclusionQuery, GL_QUERY_RESULT, &result);

						double relativeResult = (double)result / (double)(volumeRenderTarget->getWidth()*volumeRenderTarget->getHeight());
						scoreHistory.add(relativeResult);
					}
					else
					{
						double score = calculateImageScore();
						scoreHistory.add(score);
					}

				}
			}

			


		}

		vp->drawBorder();

	}


	// draw the solver score here

	if (runAlignment || !solver->getHistory().history.empty())
		drawScoreHistory(solver->getHistory());
	
	if (calculateScore)
		drawScoreHistory(scoreHistory);
	
	
}

void SpimRegistrationApp::saveStackTransformations() const
{
	std::for_each(stacks.begin(), stacks.end(), [](const SpimStack* s) 
	{ 
		std::string filename = s->getFilename() + ".registration.txt";
		s->saveTransform(filename); 
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
		s->loadTransform(filename);
	};

	updateGlobalBbox();
}

void SpimRegistrationApp::saveContrastSettings() const
{
	std::string filename(configPath + "contrast.txt");
	std::ofstream file(filename);

	if (file.is_open())
	{

		assert(file.is_open());

		std::cout << "[File] Saving contrast settings to \"" << filename << "\"\n";

		file << "min: " << (int)globalThreshold.min << std::endl;
		file << "max: " << (int)globalThreshold.max << std::endl;
	}
	else
	{
		throw std::runtime_error("Unable to write contrast settings to file \"" + filename + "\"!");
	}
}
void SpimRegistrationApp::loadContrastSettings()
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



void SpimRegistrationApp::addSpimStack(const std::string& filename)
{
	SpimStack* stack = new SpimStackU16;
	
	stack->load(filename);
	stack->subsample();

	/*
	stack->subsample(false);
	stack->subsample();
	*/

	stacks.push_back(stack);
	addInteractionVolume(stack);
	
}

void SpimRegistrationApp::addPointcloud(const std::string& filename)
{
	const glm::mat4 scaleMatrix = glm::scale(glm::vec3(100.f));


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


void SpimRegistrationApp::setPerspectiveLayout(const glm::ivec2& res, const glm::ivec2& mouseCoords)
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


void SpimRegistrationApp::setTopviewLayout(const glm::ivec2& res, const glm::ivec2& mouseCoords)
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

void SpimRegistrationApp::setThreeViewLayout(const glm::ivec2& res, const glm::ivec2& mouseCoords)
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


void SpimRegistrationApp::setContrastEditorLayout(const glm::ivec2& res, const glm::ivec2& mouseCoords)
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
	if (vp && vp->name == Viewport::PERSPECTIVE)
		vp->camera->rotate(delta.x, delta.y);

	setCameraMoving(true);

}


void SpimRegistrationApp::updateMouseMotion(const glm::ivec2& cursor)
{
	layout->updateMouseMove(cursor);
}

void SpimRegistrationApp::toggleSelectStack(int n)
{
	if (n >= interactionVolumes.size())
		currentVolume= -1;

	if (currentVolume == n)
		currentVolume = -1;
	else
		currentVolume = n;
}

void SpimRegistrationApp::toggleStack(int n)
{
	if (n >= stacks.size() || n < 0)
		return;

	stacks[n]->toggle();
}

void SpimRegistrationApp::rotateCurrentStack(float rotY)
{
	if (!currentVolumeValid())
		return;

	interactionVolumes[currentVolume]->rotate(glm::radians(rotY));
	updateGlobalBbox();
}



void SpimRegistrationApp::moveStack(const glm::vec2& delta)
{
	if (!currentVolumeValid())
		return;

	if (runAlignment)
		return;

	Viewport* vp = layout->getActiveViewport();
	if (vp && vp->name != Viewport::CONTRAST_EDITOR)
	{
		//stacks[currentStack]->move(vp->camera->calculatePlanarMovement(delta));
		interactionVolumes[currentVolume]->move(vp->camera->calculatePlanarMovement(delta));
	}
}

void SpimRegistrationApp::contrastEditorApplyThresholds()
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

void SpimRegistrationApp::changeContrast(const glm::ivec2& cursor)
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

		
		unsigned int value = globalThreshold.min + (int)(currentContrastCursor * globalThreshold.getSpread());

		if (value != lastValue)
		{
			lastValue = value;
			std::cout << "[Debug] Selected Contrast: " << value << "(" << currentContrastCursor << ")\n";


			unsigned int index = (unsigned int)(currentContrastCursor * globalThreshold.getSpread());
			for (size_t i = 0; i < histograms.size(); ++i)
			{
				std::cout << "[Histo] " << i << ": " << histograms[i][index] << std::endl;
			}


		}


	}
}

void SpimRegistrationApp::increaseMaxThreshold()
{
	globalThreshold.max += 5;
	std::cout << "[Threshold] Max: " << (int)globalThreshold.max << std::endl;

	histogramsNeedUpdate = true;
}

void SpimRegistrationApp::increaseMinThreshold()
{
	globalThreshold.min += 5;
	if (globalThreshold.min > globalThreshold.max)
		globalThreshold.min = globalThreshold.max;
	std::cout << "[Threshold] Min: " << (int)globalThreshold.min << std::endl;

	histogramsNeedUpdate = true;
}

void SpimRegistrationApp::decreaseMaxThreshold()
{
	globalThreshold.max -= 5;
	if (globalThreshold.max < globalThreshold.min)
		globalThreshold.max = globalThreshold.min;

	std::cout << "[Threshold] Max: " << (int)globalThreshold.max << std::endl;

	histogramsNeedUpdate = true;
}

void SpimRegistrationApp::decreaseMinThreshold()
{
	globalThreshold.min -= 5;
	std::cout << "[Threshold] Min: " << (int)globalThreshold.min << std::endl;

	histogramsNeedUpdate = true;
}


void SpimRegistrationApp::autoThreshold()
{
	using namespace std;

	globalThreshold.max = 0;
	globalThreshold.min = numeric_limits<unsigned short>::max();
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

	globalThreshold.min = (unsigned short)(globalThreshold.mean - 3 * globalThreshold.stdDeviation);
	globalThreshold.max = (unsigned short)(globalThreshold.mean + 3 * globalThreshold.stdDeviation);

	cout << "[Contrast] Global contrast: [" << globalThreshold.min << " -> " << globalThreshold.max << "], mean: " << globalThreshold.mean << ", std dev: " << globalThreshold.stdDeviation << std::endl;



	histogramsNeedUpdate = true;
}

void SpimRegistrationApp::toggleAllStacks()
{

	bool stat = !interactionVolumes[0]->enabled;
	for (size_t i = 0; i < interactionVolumes.size(); ++i)
		interactionVolumes[i]->enabled = stat;
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



	const float leftLimit = globalThreshold.min;
	const float rightLimit = globalThreshold.max;


	glLineWidth(2.f);
	glBegin(GL_LINES);
	
	glColor3f(1, 0, 0);
	glVertex2f(maxCursor, 0.f);
	glVertex2f(maxCursor, 1.f);

	glColor3f(1.f, 1.f, 0.f);
	glVertex2f(minCursor, 0.f);
	glVertex2f(minCursor, 1.f);

	glColor3f(0.1f, 0.1f, 0.1f);
	glVertex2f(minCursor, 0.f);
	glColor3f(1, 1, 1);
	glVertex2f(maxCursor, 1.f);

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


void SpimRegistrationApp::drawBoundingBoxes() const
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
			glMultMatrixf(glm::value_ptr(stacks[i]->transform));


			if (i == currentVolume)
				glColor3f(1, 1, 0);
			else
				glColor3f(0.6f, 0.6f, 0.6f);
			stacks[i]->getBBox().draw();

			glPopMatrix();
		}
	}

	for (size_t i = 0; i < pointclouds.size(); ++i)
	{
		glPushMatrix();
		glMultMatrixf(glm::value_ptr(pointclouds[i]->transform));


		if (i == currentVolume)
			glColor3f(1, 1, 0);
		else
			glColor3f(0.6f, 0.6f, 0.6f);
		pointclouds[i]->getBBox().draw();

		glPopMatrix();
	}

	glColor3f(0, 1, 1);
	globalBBox.draw();

	glDisableClientState(GL_VERTEX_ARRAY);
	glDepthMask(GL_TRUE);
	glEnable(GL_DEPTH_TEST);
}

void SpimRegistrationApp::drawGroundGrid(const Viewport* vp) const
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

void SpimRegistrationApp::drawViewplaneSlices(const Viewport* vp, const Shader* shader) const
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
			glm::vec4 p = mvp * stacks[i]->transform * glm::vec4(boxVerts[k], 1.f);
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

	for (size_t i = 0; i < stacks.size(); ++i)
	{
		glActiveTexture((GLenum)(GL_TEXTURE0 + i));
		glBindTexture(GL_TEXTURE_3D, stacks[i]->getTexture());

#ifdef _WIN32
		char uname[256];
		sprintf_s(uname, "volume[%d].texture", i);
		shader->setUniform(uname, (int)i);

		AABB bbox = stacks[i]->getBBox();
		sprintf_s(uname, "volume[%d].bboxMax", i);
		shader->setUniform(uname, bbox.max);
		sprintf_s(uname, "volume[%d].bboxMin", i);
		shader->setUniform(uname, bbox.min);

		sprintf_s(uname, "volume[%d].enabled", i);
		shader->setUniform(uname, stacks[i]->enabled);

		sprintf_s(uname, "volume[%d].inverseMVP", i);
		shader->setMatrix4(uname, glm::inverse(mvp * stacks[i]->transform));

#else
		char uname[256];
		sprintf(uname, "volume[%d].texture", i);
		shader->setUniform(uname, (int)i);

		AABB bbox = stacks[i]->getBBox();
		sprintf(uname, "volume[%d].bboxMax", i);
		shader->setUniform(uname, bbox.max);
		sprintf(uname, "volume[%d].bboxMin", i);
		shader->setUniform(uname, bbox.min);

		sprintf(uname, "volume[%d].enabled", i);
		shader->setUniform(uname, stacks[i]->enabled);

		sprintf(uname, "volume[%d].inverseMVP", i);
		shader->setMatrix4(uname, glm::inverse(mvp * stacks[i]->transform));
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

	shader->setUniform("maxThreshold", (int)globalThreshold.max);
	shader->setUniform("minThreshold", (int)globalThreshold.min);

	for (size_t i = 0; i < stacks.size(); ++i)
	{
		if (stacks[i]->enabled)
		{
			shader->setMatrix4("transform", stacks[i]->transform);
			
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

	shader->setUniform("maxThreshold", (float)globalThreshold.max);
	shader->setUniform("minThreshold", (float)globalThreshold.min);

	for (size_t i = 0; i < stacks.size(); ++i)
	{
		if (stacks[i]->enabled)
		{
			shader->setMatrix4("transform", stacks[i]->transform);
			
			// calculate view vector in volume coordinates
			glm::vec3 view = vp->camera->getViewDirection();
			glm::vec4(localView) = glm::inverse(stacks[i]->transform) * glm::vec4(view, 0.0);
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



void SpimRegistrationApp::raycastVolumes(const Viewport* vp, const Shader* shader) const
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
			glm::vec4 p = mvp * stacks[i]->transform * glm::vec4(boxVerts[k], 1.f);
			p /= p.w;

			minPVal = glm::min(minPVal, glm::vec3(p));
			maxPVal = glm::max(maxPVal, glm::vec3(p));

		}

	}

	maxPVal = glm::min(maxPVal, glm::vec3(1.f));
	minPVal = glm::max(minPVal, glm::vec3(-1.f));


	for (size_t i = 0; i < stacks.size(); ++i)
	{
		glActiveTexture((GLenum)(GL_TEXTURE0 + i));
		glBindTexture(GL_TEXTURE_3D, stacks[i]->getTexture());

#ifdef _WIN32

		char uname[256];
		sprintf_s(uname, "volume[%d].texture", i);
		shader->setUniform(uname, (int)i);

		AABB bbox = stacks[i]->getBBox();
		sprintf_s(uname, "volume[%d].bboxMax", i);
		shader->setUniform(uname, bbox.max);
		sprintf_s(uname, "volume[%d].bboxMin", i);
		shader->setUniform(uname, bbox.min);

		sprintf_s(uname, "volume[%d].enabled", i);
		shader->setUniform(uname, stacks[i]->enabled);

		sprintf_s(uname, "volume[%d].inverseMVP", i);
		shader->setMatrix4(uname, glm::inverse(mvp * stacks[i]->transform));

		sprintf_s(uname, "volume[%d].transform", i);
		shader->setMatrix4(uname, stacks[i]->transform);

		sprintf_s(uname, "volume[%d].inverseTransform", i);
		shader->setMatrix4(uname, glm::inverse(stacks[i]->transform));

#else
		char uname[256];
		sprintf(uname, "volume[%d].texture", i);
		shader->setUniform(uname, (int)i);

		AABB bbox = stacks[i]->getBBox();
		sprintf(uname, "volume[%d].bboxMax", i);
		shader->setUniform(uname, bbox.max);
		sprintf(uname, "volume[%d].bboxMin", i);
		shader->setUniform(uname, bbox.min);

		sprintf(uname, "volume[%d].enabled", i);
		shader->setUniform(uname, stacks[i]->enabled);

		sprintf(uname, "volume[%d].inverseMVP", i);
		shader->setMatrix4(uname, glm::inverse(mvp * stacks[i]->transform));
	
		sprintf(uname, "volume[%d].transform", i);
		shader->setMatrix4(uname, stacks[i]->transform);
	
		sprintf(uname, "volume[%d].inverseTransform", i);
		shader->setMatrix4(uname, glm::inverse(stacks[i]->transform));
#endif
	}

	shader->setUniform("minRayDist", minPVal.z);
	shader->setUniform("maxRayDist", maxPVal.z);
	shader->setUniform("inverseMVP", glm::inverse(mvp));
	shader->setUniform("cameraPos", camPos);

	// draw only the frontmost slice
	glBegin(GL_QUADS);
	float z = minPVal.z;// glm::mix(minPVal.z, maxPVal.z, 0.5f);
	glVertex3f(minPVal.x, maxPVal.y, z);
	glVertex3f(minPVal.x, minPVal.y, z);
	glVertex3f(maxPVal.x, minPVal.y, z);
	glVertex3f(maxPVal.x, maxPVal.y, z);
	glEnd();
		
	glActiveTexture(GL_TEXTURE0);

	shader->disable();
	
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

}

void SpimRegistrationApp::endAutoAlign()
{
	runAlignment = false;

	std::cout << "[Debug] Ending auto align.\n";

	// apply best transformation
	const IStackTransformationSolver::Solution bestResult = solver->getBestSolution();

	std::cout << "[Debug] Applying best result (id:" << bestResult.id << ", score: " << bestResult.score << ")... \n";
	saveVolumeTransform(currentVolume);
	interactionVolumes[currentVolume]->applyTransform(bestResult.matrix);
	updateGlobalBbox();

}

void SpimRegistrationApp::undoLastTransform()
{
	if (transformUndoChain.empty())
		return;

	
	VolumeTransform vt = transformUndoChain.back();
	vt.volume->transform = vt.matrix;

	transformUndoChain.pop_back();
	updateGlobalBbox();
}

void SpimRegistrationApp::startStackMove()
{
	if (!currentVolumeValid())
		return;

	//saveStackTransform(currentStack);
	saveVolumeTransform(currentVolume);
}

void SpimRegistrationApp::endStackMove()
{
	updateGlobalBbox();




}

void SpimRegistrationApp::saveVolumeTransform(unsigned int n)
{
	assert(n < interactionVolumes.size());

	VolumeTransform vt;
	vt.matrix = interactionVolumes[n]->transform;
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
		}
	}


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

}

void SpimRegistrationApp::maximizeViews()
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
			size_t hit = r.getClosestPoint(spc->getPoints(), spc->transform, dist);

			std::cout << "[Debug] Ray intersects point cloud " << i << " at point index: " <<hit  << ", dist: " << sqrtf(dist) << std::endl;


		}


	}
}

void SpimRegistrationApp::drawScoreHistory(const TinyHistory<double>& hist) const
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

		glVertex2d(i-offset, d);
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
		newSolver = new MultiDimensionalHillClimb;
	}

	// only switch solvers if we have created a valid one
	if (newSolver)
	{
		delete solver;
		solver = newSolver;

		cout << "[Debug] Switched solvers.\n";
	
	}
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
	for (size_t i = 0; i < renderTargetReadback.size(); ++i)
	{
		glm::vec3 color(renderTargetReadback[i]);
		value += glm::dot(color, color);
	}

	value /= renderTargetReadback.size();

	std::cout << "[Image] Read back render target score: " << value << std::endl;
	return value;
}

void SpimRegistrationApp::toggleScoreMode()
{
	if (useOcclusionQuery)
	{
		useOcclusionQuery = false;
		std::cout << "[Score] Using image metric\n";
	}
	else
	{
		useOcclusionQuery = true;
		std::cout << "[Score] Using occlusion query.\n";
	}

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