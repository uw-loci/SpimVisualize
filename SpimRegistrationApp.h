#pragma once

#include <vector>
#include <string>
#include <map>

#include "AABB.h"
#include "StackRegistration.h"
#include "Histogram.h"

class Shader;
class ILayout;
class SpimStack;
class Viewport;
class ReferencePoints;

class SpimRegistrationApp
{
public:
	SpimRegistrationApp(const glm::ivec2& resolution);
	~SpimRegistrationApp();
	

	void addSpimStack(const std::string& filename);
	void subsampleAllStacks();


	void reloadShaders();

	void drawScene();

	void resize(const glm::ivec2& newSize);
	
	inline size_t getStacksCount() const { return stacks.size(); }
	void toggleSelectStack(int n);
	void toggleStack(int n);
	inline void toggleCurrentStack() { toggleStack(currentStack); }
	void moveStack(const glm::vec2& delta);
	void rotateCurrentStack(float rotY);

	void changeContrast(const glm::ivec2& cursor);
	void setDataLimits();
	void resetDataLimits();


	void toggleAllStacks();


	void calculateHistogram();


	void TEST_extractFeaturePoints();
	void TEST_alignStacks();

	void clearRegistrationPoints();

	void updateMouseMotion(const glm::ivec2& cursor);
	

	void setPerspectiveLayout(const glm::ivec2& res, const glm::ivec2& mouseCoords);
	void setTopviewLayout(const glm::ivec2& res, const glm::ivec2& mouseCoords);
	void setThreeViewLayout(const glm::ivec2& res, const glm::ivec2& mouseCoords);
	void setContrastEditorLayout(const glm::ivec2& res, const glm::ivec2& mouseCoords);

	void rotateCamera(const glm::vec2& delta);
	void zoomCamera(float dt);
	void panCamera(const glm::vec2& delta);
	void centerCamera();


	void saveStackTransformations() const;
	void loadStackTransformations();
	
	void saveContrastSettings() const;
	void loadContrastSettings();

	inline void toggleGrid() { drawGrid = !drawGrid; }
	inline void toggleBboxes() { drawBboxes = !drawBboxes; }
	inline void toggleSlices() { drawSlices = !drawSlices; }
	
	void increaseMaxThreshold();
	void decreaseMaxThreshold();
	void increaseMinThreshold();
	void decreaseMinThreshold();
	void autoThreshold();


	inline void setConfigPath(const std::string& p) { configPath = p; }

private:
	std::string				configPath;
	
	ILayout*				layout;
	std::map<std::string, ILayout*>	prevLayouts;

	std::vector<SpimStack*>	stacks;

	AABB					globalBBox;

	Threshold				globalThreshold;


	bool					drawGrid;
	bool					drawBboxes;
	bool					drawSlices;
		
	int						currentStack;

	// how many planes/slices to draw for the volume
	unsigned int			sliceCount;

	
	Shader*					pointShader;
	Shader*					volumeShader;
	Shader*					sliceShader;
	
	ReferencePoints			refPointsA, refPointsB;


	SpimRegistrationApp(const SpimRegistrationApp&);


	Threshold				dataLimits;
	Histogram				histogram;


	void drawContrastEditor(const Viewport* vp);
	void drawScene(const Viewport* vp);
	static glm::vec3 getRandomColor(int n);

	inline bool currentStackValid() const { return currentStack > -1 && currentStack < stacks.size();  }


};