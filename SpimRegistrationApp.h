#pragma once

#include <vector>
#include <string>
#include <map>

#include <boost/utility.hpp>

#include "AABB.h"
#include "StackRegistration.h"
#include "Histogram.h"

class Framebuffer;
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
	
	void startStackMove();
	void endStackMove();


	void moveStack(const glm::vec2& delta);
	void rotateCurrentStack(float rotY);

	void changeContrast(const glm::ivec2& cursor);
	void setDataLimits();
	void resetDataLimits();


	void toggleAllStacks();


	void calculateHistogram();


	void TEST_beginAutoAlign();
	void TEST_endAutoAlign();
	void undoLastTransform();

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
	Shader*					volumeRaycaster;
	Shader*					drawQuad;;

	ReferencePoints			refPointsA, refPointsB;


	// Bead detection
	std::vector<Hourglass>	psfBeads;


	// volume-based alignment
	unsigned int			samplesPassedQuery;
	unsigned int			lastSamplesPass;

	glm::mat4				lastPassMatrix;
	bool					runAlignment;


	// stores undo transformations for all stacks
	struct StackTransform
	{
		SpimStack*		stack;
		glm::mat4		matrix;
	};
		
	std::vector<StackTransform> transformUndoChain;
		
	Framebuffer*			queryRenderTarget;


	Threshold				dataLimits;
	
	Histogram				histogram;

	void updateGlobalBbox();

	void drawContrastEditor(const Viewport* vp);
	void drawScene(const Viewport* vp);
	void drawVolumeAlignment(const Viewport* vp);

	void drawGroundGrid(const Viewport* vp) const;
	void drawBoundingBoxes() const;

	void drawAxisAlignedSlices(const Viewport* vp, const Shader* shader) const;
	void drawViewplaneSlices(const Viewport* vp, const Shader* shader) const;
	void raycastVolumes(const Viewport* vp, const Shader* shader) const;

	void drawRegistrationFeatures(const Viewport* vp) const;

	void TEST_alignStacksVolume(const Viewport* vp);
	void TEST_alignStacksSeries(const Viewport* vp);
	void TEST_alignStack(const Viewport* vp);
	unsigned long TEST_occlusionQueryStackOverlap(const Viewport* vp);

	static glm::vec3 getRandomColor(int n);

	inline bool currentStackValid() const { return currentStack > -1 && currentStack < stacks.size();  }

	void saveStackTransform(unsigned int n);

};