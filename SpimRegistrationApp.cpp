#include "SpimRegistrationApp.h"

#include "Framebuffer.h"
#include "Layout.h"
#include "Shader.h"
#include "SpimStack.h"
#include "OrbitCamera.h"
#include "BeadDetection.h"

#include <algorithm>
#include <iostream>
#include <fstream>
#include <random>
#include <thread>

#include <GL/glew.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/io.hpp>
#include <glm/gtx/transform2.hpp>
#include <GL/glut.h>

const unsigned int MIN_SLICE_COUNT = 20;
const unsigned int MAX_SLICE_COUNT = 1500;
const unsigned int STD_SLICE_COUNT = 100;

SpimRegistrationApp::SpimRegistrationApp(const glm::ivec2& res) : pointShader(nullptr), sliceShader(nullptr), volumeShader(nullptr), 
	volumeRaycaster(nullptr), drawQuad(nullptr), volumeDifferenceShader(nullptr), tonemapper(nullptr), layout(nullptr),
	drawGrid(true), drawBboxes(false), drawSlices(false), drawRegistrationPoints(false), currentStack(-1), sliceCount(100), 
	configPath("./"), cameraMoving(false), runAlignment(false), histogramsNeedUpdate(false), minCursor(0.f), maxCursor(1.f),
	subsampleOnCameraMove(false), renderMode(RENDER_VIEWPLANE_SLICES), useOcclusionQuery(false), blendMode(BLEND_ADD), 
	useImageAutoContrast(false)
{
	globalBBox.reset();
	layout = new PerspectiveFullLayout(res);

	prevLayouts["Perspective"] = layout;

	globalThreshold.min = 100;
	globalThreshold.max = 130; // std::numeric_limits<unsigned short>::max();

	resetSliceCount();
	reloadShaders();

	volumeRenderTarget = new Framebuffer(512, 512, GL_RGBA32F, GL_FLOAT);

	glGenQueries(4, occlusionQueries);
	glGenQueries(1, &singleOcclusionQuery);

}

SpimRegistrationApp::~SpimRegistrationApp()
{
	
	delete volumeRenderTarget;
	
	delete drawQuad;
	delete pointShader;
	delete sliceShader;
	delete volumeShader;
	delete volumeDifferenceShader;
	delete volumeRaycaster;


	glDeleteQueries(4, occlusionQueries);
	glDeleteQueries(1, &singleOcclusionQuery);

	for (size_t i = 0; i < stacks.size(); ++i)
		delete stacks[i];

	for (auto l = prevLayouts.begin(); l != prevLayouts.end(); ++l)
	{
		assert(l->second);
		delete l->second;
	}
}

void SpimRegistrationApp::reloadShaders()
{
	delete pointShader;
	pointShader = new Shader("shaders/points2.vert", "shaders/points2.frag");

	delete sliceShader;
	sliceShader = new Shader("shaders/slices.vert", "shaders/slices.frag");

	delete volumeShader;
	volumeShader = new Shader("shaders/volume2.vert", "shaders/volume2.frag");
		
	delete volumeRaycaster;
	volumeRaycaster = new Shader("shaders/volumeRaycast.vert", "shaders/volumeRaycast.frag");

	delete drawQuad;
	drawQuad = new Shader("shaders/drawQuad.vert", "shaders/drawQuad.frag");

	delete volumeDifferenceShader;
	volumeDifferenceShader = new Shader("shaders/volumeDist.vert", "shaders/volumeDist.frag");

	delete tonemapper;
	tonemapper = new Shader("shaders/drawQuad.vert", "shaders/tonemapper.frag");

}

void SpimRegistrationApp::switchRenderMode()
{
	switch (renderMode)
	{
	case RENDER_ALIGN:
		std::cout << "[Render] Now rendering viewplane slices\n";
		renderMode = RENDER_VIEWPLANE_SLICES;
		break;
	case RENDER_VIEWPLANE_SLICES:
		std::cout << "[Render] Now rendering alignment volumes.\n";
		renderMode = RENDER_ALIGN;
		break;

	default:
		std::cout << "[Render] Invalid rendermode.\n";
		renderMode = RENDER_ALIGN;
	}

}

void SpimRegistrationApp::switchBlendMode()
{
	if (blendMode == BLEND_ADD)
	{
		blendMode = BLEND_MAX;
		std::cout << "[Render] Blend mode max\n";
	}
	else
	{
		blendMode = BLEND_ADD;
		std::cout << "[Render] Blend mode add\n";
	}
}


void SpimRegistrationApp::draw()
{

	for (int i = 0; i < 4; ++i)
		occlusionQueryMask[i] = false;

	for (size_t i = 0; i < layout->getViewCount(); ++i)
	{
		const Viewport* vp = layout->getView(i);
		vp->setup();

		if (vp->name == Viewport::CONTRAST_EDITOR)
			drawContrastEditor(vp);
		else
		{




// test alignment shader
#if 0
			volumeRenderTarget->bind();
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


			if (drawGrid)
				drawGroundGrid(vp);

			if (useOcclusionQuery)
			{
				glBeginQuery(GL_SAMPLES_PASSED, singleOcclusionQuery);
				//glBeginQuery(GL_SAMPLES_PASSED, occlusionQueries[query]);
			}

			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);

			drawViewplaneSlices(vp, volumeDifferenceShader);

			glDisable(GL_BLEND);

			if (useOcclusionQuery)
				glEndQuery(GL_SAMPLES_PASSED);


			if (drawBboxes)
				drawBoundingBoxes();

			volumeRenderTarget->disable();

			if (!useOcclusionQuery)
			{
				// image-based metric
				float score = calculateScore(volumeRenderTarget);
				currentResult.result[vp->name] = score;
				currentResult.ready = true;
			}

			drawTexturedQuad(volumeRenderTarget->getColorbuffer());




#else


			if (runAlignment)
			{

				int query = vp->name;
				assert(query < 4);

				occlusionQueryMask[i] = true;

				const glm::mat4& mat = candidateTransforms.back();

				saveStackTransform(currentStack);
				stacks[currentStack]->applyTransform(mat);

				volumeRenderTarget->bind();
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


				if (drawGrid)
					drawGroundGrid(vp);
				
				if (useOcclusionQuery)
				{
					glBeginQuery(GL_SAMPLES_PASSED, singleOcclusionQuery);
					//glBeginQuery(GL_SAMPLES_PASSED, occlusionQueries[query]);
				}
				
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE);
				
				drawViewplaneSlices(vp, volumeDifferenceShader);

				glDisable(GL_BLEND);

				if (useOcclusionQuery)
					glEndQuery(GL_SAMPLES_PASSED);


				if (drawBboxes)
					drawBoundingBoxes();

				volumeRenderTarget->disable();

				undoLastTransform();
				
				if (!useOcclusionQuery)
				{
					// image-based metric
					float score = calculateScore(volumeRenderTarget);
					currentResult.result[vp->name] = score;
					currentResult.ready = true;
				}

				drawTexturedQuad(volumeRenderTarget->getColorbuffer());
			}
			else
			{

				volumeRenderTarget->bind();
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
				
				glEnable(GL_BLEND);
				glBlendFunc(GL_ONE, GL_ONE);
				//glBlendFunc(GL_SRC_ALPHA, GL_ONE);
				//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				
				if (blendMode == BLEND_ADD)
					glBlendEquation(GL_FUNC_ADD);
				else
					glBlendEquation(GL_MAX);
				
				/*
				glBlendEquationSeparate(GL_MAX, GL_FUNC_ADD);
				glBlendFuncSeparate(GL_ONE, GL_ONE, GL_ONE, GL_ONE);
				*/

				if (renderMode == RENDER_ALIGN)
					drawViewplaneSlices(vp, volumeDifferenceShader);
				else if (renderMode == RENDER_VIEWPLANE_SLICES)
					drawViewplaneSlices(vp, volumeShader);

				glDisable(GL_BLEND);
				glBlendEquation(GL_FUNC_ADD);

				volumeRenderTarget->disable();


				drawTonemappedQuad(volumeRenderTarget);


				if (drawGrid)
					drawGroundGrid(vp);

				if (drawBboxes)
					drawBoundingBoxes();


				//drawTexturedQuad(volumeRenderTarget->getColorbuffer());
			}


#endif

			
			// the query result should be done by now
			if (runAlignment && useOcclusionQuery)
			{
				GLint queryStatus = GL_FALSE;
				while (queryStatus == GL_FALSE)
					glGetQueryObjectiv(singleOcclusionQuery, GL_QUERY_RESULT_AVAILABLE, &queryStatus);

				GLuint64 result = 0;
				glGetQueryObjectui64v(singleOcclusionQuery, GL_QUERY_RESULT, &result);

				currentResult.result[vp->name] = result;
				currentResult.ready = true;
			}
		}

		vp->drawBorder();

	}



	/*
	// gather all occlusion queries here
	if (runAlignment)
	{
		
		auto start = std::chrono::high_resolution_clock::now();

		for (int i = 0; i < 4; ++i)
		{

			// read back occlusion result
			if (occlusionQueryMask[i])
			{
				GLint queryStatus = GL_FALSE;
				while (queryStatus == GL_FALSE)
					glGetQueryObjectiv(singleOcclusionQuery, GL_QUERY_RESULT_AVAILABLE, &queryStatus);

				GLuint64 result = 0;
				glGetQueryObjectui64v(singleOcclusionQuery, GL_QUERY_RESULT, &result);

				currentResult.result[i] = result;
			}
		}
		
		
		auto end = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double, std::milli> elapsed = end - start;
		std::cout << "[Debug] Read back occlusion query in " << elapsed.count() << " ms.\n";



	}
	*/

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
	for (unsigned int i = 0; i < stacks.size(); ++i)
	{
		saveStackTransform(i);

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
	SpimStack* stack = new SpimStack(filename);

	//stack->subsample(false);
	stack->subsample();

	stacks.push_back(stack);
	AABB bbox = stack->getBBox();

	if (stacks.size() == 1)
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


void SpimRegistrationApp::drawTonemappedQuad(Framebuffer* fbo) const
{

	// read back render target to determine largest and smallest value
	bool readback = false;
	if (readback)
	{
		fbo->bind();
		glReadBuffer(GL_COLOR_ATTACHMENT0);

		std::vector<glm::vec4> pixels(fbo->getWidth()*fbo->getHeight());
		glReadPixels(0, 0, fbo->getWidth(), fbo->getHeight(), GL_RGBA, GL_FLOAT, glm::value_ptr(pixels[0]));
		fbo->disable();

		glm::vec4 minVal(std::numeric_limits<float>::max());
		glm::vec4 maxVal(std::numeric_limits<float>::lowest());

		for (size_t i = 0; i < pixels.size(); ++i)
		{
			minVal = glm::min(minVal, pixels[i]);
			maxVal = glm::max(maxVal, pixels[i]);
		}

		std::cout << "[Debug] Read back min: " << minVal << ", max: " << maxVal << std::endl;

		glReadBuffer(GL_BACK);
	}

	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);

	tonemapper->bind();
	tonemapper->setUniform("maxThreshold", (float)globalThreshold.max);
	tonemapper->setUniform("minThreshold", (float)globalThreshold.min);

	if (useImageAutoContrast)
	{
		tonemapper->setUniform("minThreshold", minImageContrast);
		tonemapper->setUniform("maxThreshold", maxImageContrast);
	}


	tonemapper->setUniform("sliceCount", (float)sliceCount);
	tonemapper->setTexture2D("colormap", fbo->getColorbuffer());

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




glm::vec3 SpimRegistrationApp::getRandomColor(int n)
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
		if (currentStack == -1)
			vp->camera->target = globalBBox.getCentroid();
		else
			vp->camera->target = stacks[currentStack]->getTransformedBBox().getCentroid();
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
	if (n >= stacks.size())
		currentStack = -1;

	if (currentStack == n)
		currentStack = -1;
	else
		currentStack = n;
}

void SpimRegistrationApp::toggleStack(int n)
{
	if (n >= stacks.size() || n < 0)
		return;

	stacks[n]->toggle();
}

void SpimRegistrationApp::rotateCurrentStack(float rotY)
{
	if (!currentStackValid())
		return;

	stacks[currentStack]->rotate(rotY);
	updateGlobalBbox();
}



void SpimRegistrationApp::moveStack(const glm::vec2& delta)
{
	if (!currentStackValid())
		return;

	if (runAlignment)
		return;

	Viewport* vp = layout->getActiveViewport();
	if (vp && vp->name != Viewport::CONTRAST_EDITOR)
	{
		stacks[currentStack]->move(vp->camera->calculatePlanarMovement(delta));
	}
}

void SpimRegistrationApp::contrastEditorApplyThresholds()
{
	unsigned short newMin = globalThreshold.min + minCursor * globalThreshold.getSpread();
	unsigned short newMax = globalThreshold.min + maxCursor * globalThreshold.getSpread();

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


			unsigned int index = currentContrastCursor * globalThreshold.getSpread();
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

	globalThreshold.min = globalThreshold.mean - 3 * globalThreshold.stdDeviation;
	globalThreshold.max = globalThreshold.mean + 3 * globalThreshold.stdDeviation;

	cout << "[Contrast] Global contrast: [" << globalThreshold.min << " -> " << globalThreshold.max << "], mean: " << globalThreshold.mean << ", std dev: " << globalThreshold.stdDeviation << std::endl;



	histogramsNeedUpdate = true;
}

void SpimRegistrationApp::toggleAllStacks()
{
	bool stat = !stacks[0]->enabled;
	for (size_t i = 0; i < stacks.size(); ++i)
		stacks[i]->enabled = stat;
}

void SpimRegistrationApp::subsampleAllStacks()
{
	for (auto it = stacks.begin(); it != stacks.end(); ++it)
		(*it)->subsample(true);
}

void SpimRegistrationApp::calculateHistograms()
{
	histograms.clear();

	size_t maxVal = 0.f;

	for (size_t i = 0; i < stacks.size(); ++i)
	{
		std::vector<size_t> histoRaw = stacks[i]->calculateHistogram(globalThreshold);

		for (size_t j = 0; j < histoRaw.size(); ++j)
			maxVal = std::max(maxVal, histoRaw[j]);


		std::vector<float> histoFloat(histoRaw.begin(), histoRaw.end());
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

	glColor3f(1, 1, 0);
	glVertex2f(minCursor, 0.f);
	glVertex2f(minCursor, 1.f);

	glColor3f(0.1, 0.1, 0.1);
	glVertex2f(minCursor, 0.f);
	glColor3f(1, 1, 1);
	glVertex2f(maxCursor, 1.f);

	glEnd();
	
	glLineWidth(1.f);





	// draw histogram
	
	glBegin(GL_LINES);
	for (size_t i = 0; i < histograms.size(); ++i)
	{
		glm::vec3 rc = getRandomColor(i);
		glColor3fv(glm::value_ptr(rc));
	

		for (unsigned int ix = 0; ix < histograms[i].size(); ++ix)
		{
			float x = (float)ix / histograms[i].size();

			// offset each value slightly
			//x += ((float)histograms.size() / (float)vp->size.x);
			x += 0.1;

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


			if (i == currentStack)
				glColor3f(1, 1, 0);
			else
				glColor3f(0.6, 0.6, 0.6);
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
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_3D, stacks[i]->getTexture());

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
	}

	// draw all slices
	glBegin(GL_QUADS);	
	for (int z = 0; z < sliceCount; ++z)
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
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_3D, stacks[i]->getTexture());

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
	if (stacks.size() < 2 || currentStack == -1)
		return;

	if (runAlignment)
		return;


	std::cout << "[Debug] Aligning stack " << currentStack << " to stack 0 ... " << std::endl;
	

	runAlignment = true;      
	if (candidateTransforms.empty())
		createCandidateTransforms();
	

	lastSamplesPass = 0;
	lastPassMatrix = stacks[currentStack]->transform;


	saveStackTransform(currentStack);
}

void SpimRegistrationApp::endAutoAlign()
{

	runAlignment = false;
	lastSamplesPass = 0;

	
	// clear all pending stuff
	//candidateTransforms.clear();
	occlusionQueryResults.clear();
	
	
}

void SpimRegistrationApp::undoLastTransform()
{
	if (transformUndoChain.empty())
		return;

	
	StackTransform st = transformUndoChain.back();
	st.stack->transform = st.matrix;

	transformUndoChain.pop_back();
	updateGlobalBbox();
}

void SpimRegistrationApp::startStackMove()
{
	if (currentStack == -1)
		return;

	saveStackTransform(currentStack);
}

void SpimRegistrationApp::endStackMove()
{
	updateGlobalBbox();
}

void SpimRegistrationApp::saveStackTransform(unsigned int n)
{
	assert(n < stacks.size());

	StackTransform st;
	st.matrix = stacks[n]->transform;
	st.stack = stacks[n];

	transformUndoChain.push_back(st);
}

void SpimRegistrationApp::updateGlobalBbox()
{
	if (stacks.empty())
	{
		globalBBox.reset();
		return;
	}
	
	globalBBox = stacks[0]->getTransformedBBox();
	for (size_t i = 1; i < stacks.size(); ++i)
		globalBBox.extend(stacks[i]->getTransformedBBox());


}

void SpimRegistrationApp::update(float dt)
{
	using namespace std;

	if (runAlignment)
	{
		std::cout << "[Align] Testing transform " << candidateTransforms.size() << " ... \n";

		// remove the last frame's transform
		candidateTransforms.pop_back();

		if (candidateTransforms.empty())
		{		
			std::cout << "[Debug] Selecting best transform ... \n";
			selectAndApplyBestTransform();
		
		
			createCandidateTransforms();
			runAlignment = false;
		}


		if (currentResult.ready)
		{
			occlusionQueryResults.push_back(currentResult);

			// reset the current result
			currentResult.result[0] = 0;
			currentResult.result[1] = 0;
			currentResult.result[2] = 0;
			currentResult.result[3] = 0;
			currentResult.ready = false;
			currentResult.matrix = candidateTransforms.back();
		}
		
		/*
		// first process all existing transforms
		if (occlusionQueryResults.size() >= 10)
			selectAndApplyBestTransform();
		*/
		
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
	if (currentStackValid())
	{
		for (auto it = prevLayouts.begin(); it != prevLayouts.end(); ++it)
			it->second->maximizeView(stacks[currentStack]->getBBox());

	}
	else
	{
		for (auto it = prevLayouts.begin(); it != prevLayouts.end(); ++it)
			it->second->maximizeView(globalBBox);
	}
	
	
}


void SpimRegistrationApp::createCandidateTransforms()
{
	for (int y = -2; y <= 2; ++y)
	{

		for (int x = -5; x <= 5; ++x)
		{
			for (int z = -5; z <= 5; ++z)
			{
				glm::vec3 v(x, y, z);
				v /= 10.f;

				glm::mat4 T = glm::translate(v);


				for (int ry = -3; ry <= 3; ++ry)
				{
					float angle = ry / 10.f;
					glm::mat4 R = glm::rotate(angle, glm::vec3(0, 1, 0));

					candidateTransforms.push_back(R*T);
				}




			}

		}
	}


	std::mt19937 rng;
	std::shuffle(candidateTransforms.begin(), candidateTransforms.end(), rng);

	std::cout << "[Debug] Created " << candidateTransforms.size() << " new candidate transforms.\n";
	
	// reset the current result
	currentResult.result[0] = 0;
	currentResult.result[1] = 0;
	currentResult.result[2] = 0;
	currentResult.result[3] = 0;
	currentResult.ready = false;
	currentResult.matrix = candidateTransforms.back();
}


void SpimRegistrationApp::selectAndApplyBestTransform()
{
	if (occlusionQueryResults.empty())
		return;

	sort(occlusionQueryResults.begin(), occlusionQueryResults.end());
	const OcclusionQueryResult& bestResult = occlusionQueryResults.back();
	

	if (bestResult.getScore() > lastSamplesPass)
	{
		// apply transform
		stacks[currentStack]->applyTransform(bestResult.matrix);
		std::cout << "[Debug] Selected transform with a score of " << bestResult.getScore() << std::endl;


		lastSamplesPass = bestResult.getScore();
	}
	else
		std::cout << "[Debug] No transform with better samples passed than before!\n";
	
	occlusionQueryResults.clear();
}


void SpimRegistrationApp::toggleSlices()
{
	drawSlices = !drawSlices;
	std::cout << "[Render] Drawing " << (drawSlices ? "slices" : "volumes") << std::endl;
}

double SpimRegistrationApp::calculateScore(Framebuffer* fbo) const
{
	fbo->bind();
	glReadBuffer(GL_COLOR_ATTACHMENT0);

	std::vector<glm::vec4> pixels(fbo->getWidth()*fbo->getHeight());
	glReadPixels(0, 0, fbo->getWidth(), fbo->getHeight(), GL_RGBA, GL_FLOAT, glm::value_ptr(pixels[0]));
	fbo->disable();

	double value = 0;

	for (size_t i = 0; i < pixels.size(); ++i)
	{
		glm::vec3 color(pixels[i]);
		value += glm::dot(color, color);
	}

	std::cout << "[Debug] Read back score: " << value << std::endl;
	glReadBuffer(GL_BACK);

	return value;
}

void SpimRegistrationApp::inspectOutputImage(const glm::ivec2& cursor)
{
	using namespace glm;

	// only work with fullscreen layouts for now 
	if (!layout->isSingleView())
		return;

	if (currentStackValid())
		return;


	// read back last render target
	// TODO: change me to the _correct_ render target
	volumeRenderTarget->bind();
	glReadBuffer(GL_COLOR_ATTACHMENT0);

	std::vector<vec4> pixels(volumeRenderTarget->getWidth()*volumeRenderTarget->getHeight());
	glReadPixels(0, 0, volumeRenderTarget->getWidth(), volumeRenderTarget->getHeight(), GL_RGBA, GL_FLOAT, glm::value_ptr(pixels[0]));
	volumeRenderTarget->disable();

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
		std::cout << "[Image] " << pixels[index] << std::endl;
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
	sliceCount *= 1.4;
	sliceCount = std::min(sliceCount, MAX_SLICE_COUNT);
	std::cout << "[Slices] Slicecount: " << sliceCount << std::endl;
}


void SpimRegistrationApp::decreaseSliceCount()
{
	sliceCount /= 1.4;
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