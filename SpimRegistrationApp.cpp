#include "SpimRegistrationApp.h"

#include "Layout.h"
#include "Shader.h"
#include "SpimStack.h"
#include "OrbitCamera.h"
#include "BeadDetection.h"

#include <algorithm>
#include <iostream>
#include <fstream>
#include <random>


#include <GL/glew.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/io.hpp>
#include <glm/gtx/transform2.hpp>

SpimRegistrationApp::SpimRegistrationApp(const glm::ivec2& res) : pointShader(0), sliceShader(0), volumeShader(0), layout(0), 
	drawGrid(true), drawBboxes(false), drawSlices(false), drawRegistrationPoints(false), currentStack(-1), sliceCount(400), 
	configPath("./"), cameraMoving(false), runAlignment(false)
{
	globalBBox.reset();
	layout = new PerspectiveFullLayout(res);

	prevLayouts["Perspective"] = layout;

	globalThreshold.min = 100;
	globalThreshold.max = 130; // std::numeric_limits<unsigned short>::max();

	reloadShaders();

	glGenQueries(1, &samplesPassedQuery);
}

SpimRegistrationApp::~SpimRegistrationApp()
{
	delete pointShader;
	delete sliceShader;
	delete volumeShader;
	delete volumeDifferenceShader;
	delete volumeRaycaster;

	glDeleteQueries(1, &samplesPassedQuery);

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
	sliceShader = new Shader("shaders/volume.vert", "shaders/volume.frag");

	delete volumeShader;
	volumeShader = new Shader("shaders/volume2.vert", "shaders/volume2.frag");

	delete volumeDifferenceShader;
	volumeDifferenceShader = new Shader("shaders/volumeDist.vert", "shaders/volumeDist.frag");

	delete volumeRaycaster;
	volumeRaycaster = new Shader("shaders/volumeRaycast.vert", "shaders/volumeRaycast.frag");
}


void SpimRegistrationApp::draw()
{
	for (size_t i = 0; i < layout->getViewCount(); ++i)
	{
		const Viewport* vp = layout->getView(i);
		vp->setup();
		
		if (vp->name == Viewport::CONTRAST_EDITOR)
			drawContrastEditor(vp);
		else if (vp->name == Viewport::PERSPECTIVE_ALIGNMENT)
			drawVolumeAlignment(vp);
		else
			drawScene(vp);
	}

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
		SpimStack* s = stacks[i];

		// save the current matrix
		StackTransform st;
		st.matrix = s->transform;
		st.stack = i;
		transformUndoChain.push_back(st);		


		std::string filename = s->getFilename() + ".registration.txt";
		s->loadTransform(filename);
	};
}

void SpimRegistrationApp::saveContrastSettings() const
{
	std::string filename(configPath + "contrast.txt");
	std::ofstream file(filename);
	assert(file.is_open());

	std::cout << "[File] Saving contrast settings to \"" << filename << "\"\n";

	file << "min: " << (int)globalThreshold.min << std::endl;
	file << "max: " << (int)globalThreshold.max << std::endl;
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


	stacks.push_back(stack);
	AABB bbox = stack->getBBox();
	
	if (stacks.size() == 1)
		globalBBox = bbox;
	else
		globalBBox.extend(bbox);
}

void SpimRegistrationApp::drawScene(const Viewport* vp)
{

	if (drawGrid)
		drawGroundGrid(vp);

	

	if (drawSlices)
	{
		drawAxisAlignedSlices(vp, sliceShader);
	}
	else
	{
		drawViewplaneSlices(vp, volumeShader);
	}
	

	if (drawBboxes)
	{
		drawBoundingBoxes();

	}

	/*
	if (drawRegistrationPoints)
		drawRegistrationFeatures(vp);
	*/
}

void SpimRegistrationApp::drawVolumeAlignment(const Viewport* vp)
{
	if (vp->name != Viewport::PERSPECTIVE_ALIGNMENT)
		return;

	if (runAlignment)
		TEST_alignStack(vp);


	if (drawGrid)
		drawGroundGrid(vp);


	static bool queryReady = true;
	
	if (queryReady)
		glBeginQuery(GL_SAMPLES_PASSED, samplesPassedQuery);

	drawViewplaneSlices(vp, volumeDifferenceShader);
	
	glEndQuery(GL_SAMPLES_PASSED);
	


	GLint queryStatus = GL_FALSE;
	while (queryStatus == GL_FALSE)
	{
		glGetQueryObjectiv(samplesPassedQuery, GL_QUERY_RESULT_AVAILABLE, &queryStatus);
	}


	GLuint64 samplesPassed = 0;
	glGetQueryObjectui64v(samplesPassedQuery, GL_QUERY_RESULT, &samplesPassed);
	
	if (runAlignment)
	{

		// undo transform if we are worse off
		if (samplesPassed < lastSamplesPass)
		{
			stacks[1]->transform = lastPassMatrix;

		}
		else
			lastSamplesPass = samplesPassed;

	}
			
	std::cout << "[Debug] Samples passed: " << samplesPassed << " (" << lastSamplesPass << ")" << std::endl;
	queryReady = true;

	
	
	
	
	
	//raycastVolumes(vp, volumeRaycaster);



	
	
	if (drawBboxes)
		drawBoundingBoxes();

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

void SpimRegistrationApp::setAlignVolumeLayout(const glm::ivec2& res, const glm::ivec2& mouseCoords)
{
	std::cout << "[Layout] Creating volume aligning layout ... \n";
	if (prevLayouts["Align Volumes"])
	{
		layout = prevLayouts["Align Volumes"];
		layout->resize(res);
	}
	else
	{
		layout = new AlignVolumesLayout(res);
		prevLayouts["Align Volumes"] = layout;
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
			vp->camera->target = stacks[currentStack]->getBBox().getCentroid();
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
	if (vp && (vp->name == Viewport::PERSPECTIVE || vp->name == Viewport::PERSPECTIVE_ALIGNMENT))
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
}



void SpimRegistrationApp::moveStack(const glm::vec2& delta)
{
	if (!currentStackValid())
		return;

	Viewport* vp = layout->getActiveViewport();
	if (vp)// && vp->name != Viewport::PERSPECTIVE)
	{
		stacks[currentStack]->move(vp->camera->calculatePlanarMovement(delta));
	}
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

		

		const float leftLimit = dataLimits.min;
		const float rightLimit = dataLimits.max;
		
		float minBar = globalThreshold.min / rightLimit;
		float maxBar = globalThreshold.max / rightLimit;
		
		if (abs(coords.x - minBar) < abs(coords.x - maxBar))
		{
			// min bar is closer to cursor -- move this
			unsigned short val = (unsigned short)(dataLimits.max*coords.x);
			globalThreshold.min = val;
		

			std::cout << "[Contrast] min val:" << val << " (" << minBar << ")\n";
		}
		else
		{
			// max bar is closer to cursor -- move this
			unsigned short val = (unsigned short)(dataLimits.max*coords.x);
			globalThreshold.max = val;

			std::cout << "[Contrast] max val:" << val << " (" << maxBar << ")\n";
		}
		


	}
}


void SpimRegistrationApp::TEST_extractFeaturePoints()
{

	stacks[0]->extractTransformedFeaturePoints(globalThreshold, refPointsA);
	stacks[1]->extractTransformedFeaturePoints(globalThreshold, refPointsB);











	/*
	using namespace glm;
	if (stacks.size() < 2)
		return;

	refPointsA.setPoints(stacks[0]->extractTransformedPoints(stacks[1], globalThreshold));
	refPointsB.setPoints(stacks[1]->extractTransformedPoints(stacks[0], globalThreshold));

	std::cout << "[Align] Extracted " << refPointsA.size() << " and\n";
	std::cout << "[Align]           " << refPointsB.size() << " points.\n";


	if (refPointsA.empty() || refPointsB.empty())
	{
		std::cout << "[Aling] Either data set is empty, aborting.\n";
		return;
	}



	if (refPointsA.size() > refPointsB.size())
		refPointsA.trim(&refPointsB);
	else
		refPointsB.trim(&refPointsA);

	*/

}

void SpimRegistrationApp::TEST_alignStacks()
{
	



	/*
	using namespace glm;

	if (stacks.size() < 2)
		return;

	if (refPointsA.empty() || refPointsB.empty())
	{
		std::cout << "[Aling] Either data set is empty, aborting.\n";
		return;
	}


	std::cout << "[Align] Mean distance before alignment: " << refPointsA.calculateMeanDistance(&refPointsB) << std::endl;

	mat4 delta(1.f);
	refPointsA.align(&refPointsB, delta);

	std::cout << delta << std::endl;

	stacks[1]->applyTransform(delta);
	refPointsB.applyTransform(delta);


	std::cout << "[Align] Mean distance after alignment: " << refPointsA.calculateMeanDistance(&refPointsB) << std::endl;
	*/
}

void SpimRegistrationApp::increaseMaxThreshold()
{
	globalThreshold.max += 10;
	std::cout << "[Threshold] Max: " << (int)globalThreshold.max << std::endl;
}

void SpimRegistrationApp::increaseMinThreshold()
{
	globalThreshold.min += 10;
	std::cout << "[Threshold] Min: " << (int)globalThreshold.min << std::endl;
}

void SpimRegistrationApp::decreaseMaxThreshold()
{
	globalThreshold.max -= 10;
	std::cout << "[Threshold] Max: " << (int)globalThreshold.max << std::endl;
}

void SpimRegistrationApp::decreaseMinThreshold()
{
	globalThreshold.min -= 10;
	std::cout << "[Threshold] Min: " << (int)globalThreshold.min << std::endl;
}


void SpimRegistrationApp::autoThreshold()
{
	globalThreshold.max = 0;
	globalThreshold.min = std::numeric_limits<unsigned short>::max();

	for (size_t i = 0; i < stacks.size(); ++i)
	{
		Threshold l = stacks[i]->getLimits();


		std::cout << "[Contrast] Stack " << i << " contrast: " << (int)l.min << " -> " << (int)l.max << std::endl;

		globalThreshold.max = std::max(globalThreshold.max, l.max);
		globalThreshold.min = std::min(globalThreshold.min, l.min);
	}


	std::cout << "[Contrast] Set global thresholds to " << (int)globalThreshold.min << " -> " << (int)globalThreshold.max << std::endl;
}

void SpimRegistrationApp::clearRegistrationPoints()
{
	refPointsA.clear();
	refPointsB.clear();
	std::cout << "[Registration] Clearing registration points.\n";
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

void SpimRegistrationApp::calculateHistogram()
{
	if (stacks.empty())
		return;


	std::cout << "[Histo] Calculating histogram for stack 0 ... ";

	dataLimits = stacks[0]->getLimits();
	histogram = stacks[0]->calculateHistogram(globalThreshold);
	std::cout << "done.\n";
}

void SpimRegistrationApp::drawContrastEditor(const Viewport* vp)
{
	if (histogram.empty())
	{
		calculateHistogram();
	}

	/*
	glColor3f(0.7f, 0.7f, 0.7f);
	glBegin(GL_QUADS);
	glVertex2i(0, 0);
	glVertex2i(1, 0);
	glVertex2i(1, 1);
	glVertex2i(0, 1);
	glEnd();
	*/

	glColor3f(1,1,1);
	glBegin(GL_LINE_LOOP);
	glVertex2i(0, 0);
	glVertex2i(1, 0);
	glVertex2i(1, 1);
	glVertex2i(0, 1);
	glEnd();



	const float leftLimit = 0.f;
	const float rightLimit = dataLimits.max;


	glLineWidth(2.f);
	glBegin(GL_LINES);
	glColor3f(1, 1, 0);
	glVertex2f((float)globalThreshold.min / rightLimit, 0);
	glVertex2f((float)globalThreshold.min / rightLimit, 1);

	glColor3f(1, 0.5, 0);
	glVertex2f((float)globalThreshold.max / rightLimit, 0);
	glVertex2f((float)globalThreshold.max / rightLimit, 1);
	
	
	glEnd();

	
	glLineWidth(1.f);


	// draw histogram
	if (!histogram.empty())
	{
		float dx = 1.f / histogram.bins.size();
		float dy = 1.f / histogram.max;

		glBegin(GL_LINES);
		glColor3f(0.7f, 0.7f, 0.7f);

		for (int i = 0; i < histogram.bins.size(); ++i)
		{
			glVertex2f(dx*i, 0.f);
			glVertex2f(dx*i, dy*log((float)histogram.bins[i]));


		}

		glEnd();

	}
	


	/*
	glBegin(GL_LINES);
		
	glColor3f(0.7f, 0.7f, 0.7f);

	glVertex2i(0, 0);
	glVertex2i(histogram.bins.size(), 0);

	glColor3f(1.f, 1.f, 1.f);
	for (unsigned int i = 0; i < histogram.bins.size(); ++i)
	{
		unsigned int x = i * histogram.binSize * histogram.lowest;

		glVertex3i(x, 0, 0);
		glVertex3f(x, log((float)histogram.bins[i]), 0);
	}
	
	glColor3f(1, 1, 0);
	glVertex3i(globalThreshold.min, 0, 0);
	glVertex3i(globalThreshold.min, 1, 0);

	glColor3f(1, 0.5, 0);
	glVertex3i(globalThreshold.max, 0, 0);
	glVertex3i(globalThreshold.max, 1, 0);

	glEnd();
	*/
	/*
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	*/

}

void SpimRegistrationApp::setDataLimits()
{
	dataLimits = globalThreshold;
}

void SpimRegistrationApp::resetDataLimits()
{
	dataLimits = stacks[0]->getLimits();
}

void SpimRegistrationApp::TEST_detectBeads()
{
	assert(!stacks.empty());
	stacks[0]->detectHourglasses();
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

	glDisableClientState(GL_VERTEX_ARRAY);
	glDepthMask(GL_TRUE);
	glEnable(GL_DEPTH_TEST);
}

void SpimRegistrationApp::drawGroundGrid(const Viewport* vp) const
{
	if (vp->name == Viewport::PERSPECTIVE || vp->name == Viewport::ORTHO_Y || vp->name == Viewport::PERSPECTIVE_ALIGNMENT)
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

	if (vp->name == Viewport::ORTHO_X)
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

	if (vp->name == Viewport::ORTHO_Z)
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

void SpimRegistrationApp::drawViewplaneSlices(const Viewport* vp, const Shader* shader) const
{
	glm::mat4 mvp;// = vp.proj * vp.view;
	vp->camera->getMVP(mvp);



	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	shader->bind();
	shader->setUniform("minThreshold", globalThreshold.min);
	shader->setUniform("maxThreshold", globalThreshold.max);
	
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
	}



	unsigned int slices = sliceCount;
	if (cameraMoving)
		slices = sliceCount / 10;

	shader->setUniform("sliceCount", (float)slices);


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

	shader->disable();
	glDisable(GL_BLEND);

}

void SpimRegistrationApp::drawAxisAlignedSlices(const Viewport* vp, const Shader* shader) const
{
	glm::mat4 mvp;// = vp.proj * vp.view;
	vp->camera->getMVP(mvp);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);

	shader->bind();
	shader->setUniform("minThreshold", (int)globalThreshold.min);
	shader->setUniform("maxThreshold", (int)globalThreshold.max);
	shader->setUniform("mvpMatrix", mvp);

	for (size_t i = 0; i < stacks.size(); ++i)
	{
		if (stacks[i]->enabled)
		{

			shader->setUniform("color", getRandomColor(i));
			shader->setMatrix4("transform", stacks[i]->transform);


			glColor4f(0.f, 0.f, 0.f, 1.f);

			// calculate view vector
			glm::vec3 view = vp->camera->getViewDirection();
			stacks[i]->drawSlices(volumeShader, view);

		}


	}

	shader->disable();

	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
}

void SpimRegistrationApp::drawRegistrationFeatures(const Viewport* vp) const
{

	// draw beads here
	{
		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);

		glActiveTexture(GL_TEXTURE0);
		glDisable(GL_BLEND);

		glEnableClientState(GL_VERTEX_ARRAY);
		glDisableClientState(GL_COLOR_ARRAY);


		if (!psfBeads.empty())
		{
			for (size_t i = 0; i < psfBeads.size(); ++i)
			{
				psfBeads[i].draw();
			}

		}



		glDisableClientState(GL_VERTEX_ARRAY);
		glDepthMask(GL_TRUE);
		glEnable(GL_DEPTH_TEST);
	}



	// draw registration points
	{
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_COLOR_ARRAY);

		if (!refPointsA.empty())
		{
			//std::cout << "[Debug] Drawing " << TEST_points.size() << " points.\n";


			// draw points
			glColor3f(1.f, 0.f, 0.f);
			refPointsA.draw();
			/*
			glColor3f(0.f, 1.f, 0.f);
			refPointsB.draw();
			*/

		}

		if (!refPointsB.empty())
			refPointsB.draw();

		glDisableClientState(GL_VERTEX_ARRAY);
		glDisableClientState(GL_COLOR_ARRAY);
	}
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


void SpimRegistrationApp::TEST_beginAutoAlign()
{
	if (stacks.size() < 2 || currentStack == -1)
		return;

	std::cout << "[Debug] Aligning stack " << currentStack << " ... " << std::endl;

	runAlignment = true;

	StackTransform st;
	st.stack = currentStack;
	st.matrix = stacks[currentStack]->transform;

}

void SpimRegistrationApp::TEST_endAutoAlign()
{
	runAlignment = false;
	lastSamplesPass = 0;
}

void SpimRegistrationApp::TEST_alignStack(const Viewport* vp)
{


	static std::mt19937 rng;
	static std::uniform_real<float> rngDist(-1.f, 1.f);
	
	// 1) get camera frame
	

	// 2) create delta matrix
	float dx = rngDist(rng);
	float dz = rngDist(rng);
	float ry = rngDist(rng);
	
	glm::mat4 R = glm::rotate(ry, glm::vec3(0, 1, 0));
	glm::mat4 T = glm::translate(glm::vec3(dx, 0, dz));
	glm::mat4 M = R * T;

	std::cout << "[Debug] Delta T: " << M << std::endl;


	lastPassMatrix = stacks[currentStack]->transform;

	// 3) apply matrix to stack
	assert(stacks.size() > 1);
	stacks[currentStack]->applyTransform(M);


	// 4) render frame stack and record value

}

void SpimRegistrationApp::undoLastTransform()
{
	if (transformUndoChain.empty())
		return;

	StackTransform st(transformUndoChain.back());
	transformUndoChain.pop_back();


	assert(st.stack < stacks.size());
	stacks[st.stack]->transform = st.matrix;
}

void SpimRegistrationApp::startStackMove()
{
	if (currentStack == -1)
		return;

	StackTransform st;
	st.matrix = stacks[currentStack]->transform;
	st.stack = currentStack;
	transformUndoChain.push_back(st);
}

void SpimRegistrationApp::endStackMove()
{
}