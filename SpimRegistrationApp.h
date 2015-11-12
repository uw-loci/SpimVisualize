#pragma once

#include <vector>
#include <string>
#include <map>

#include <boost/utility.hpp>

#include "AABB.h"
#include "StackRegistration.h"
#include "Histogram.h"

class Shader;
class ILayout;
class SpimStack;
class Viewport;
class ReferencePoints;
struct Hourglass;

class SpimRegistrationApp : boost::noncopyable
{
public:
	SpimRegistrationApp(const glm::ivec2& resolution);
	~SpimRegistrationApp();
	

	void addSpimStack(const std::string& filename);
	void subsampleAllStacks();


	void reloadShaders();

	void draw();

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
	void TEST_detectBeads();

	void clearRegistrationPoints();

	void updateMouseMotion(const glm::ivec2& cursor);
	

	void setPerspectiveLayout(const glm::ivec2& res, const glm::ivec2& mouseCoords);
	void setTopviewLayout(const glm::ivec2& res, const glm::ivec2& mouseCoords);
	void setThreeViewLayout(const glm::ivec2& res, const glm::ivec2& mouseCoords);
	void setContrastEditorLayout(const glm::ivec2& res, const glm::ivec2& mouseCoords);
	void setAlignVolumeLayout(const glm::ivec2& res, const glm::ivec2& mouseCoords);

	void rotateCamera(const glm::vec2& delta);
	void zoomCamera(float dt);
	void panCamera(const glm::vec2& delta);
	void centerCamera();

	inline void setCameraMoving(bool m) { cameraMoving = m; }


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

	bool					cameraMoving;


	bool					drawGrid;
	bool					drawBboxes;
	bool					drawSlices;
	bool					drawRegistrationPoints;

	int						currentStack;

	// how many planes/slices to draw for the volume
	unsigned int			sliceCount;
	
	Shader*					pointShader;
	Shader*					volumeShader;
	Shader*					sliceShader;

	Shader*					volumeDifferenceShader;
	
	ReferencePoints			refPointsA, refPointsB;


	// Bead detection
	std::vector<Hourglass>	psfBeads;


	// volume-based alignment
	unsigned int			samplesPassedQuery;


	Threshold				dataLimits;
	Histogram				histogram;


	void drawContrastEditor(const Viewport* vp);
	void drawScene(const Viewport* vp);
	void drawVolumeAlignment(const Viewport* vp);

	void drawGroundGrid(const Viewport* vp) const;
	void drawBoundingBoxes() const;

	void drawAxisAlignedSlices(const Viewport* vp, const Shader* shader) const;
	void drawViewplaneSlices(const Viewport* vp, const Shader* shader) const;

	void drawRegistrationFeatures(const Viewport* vp) const;

	static glm::vec3 getRandomColor(int n);

	inline bool currentStackValid() const { return currentStack > -1 && currentStack < stacks.size();  }


};