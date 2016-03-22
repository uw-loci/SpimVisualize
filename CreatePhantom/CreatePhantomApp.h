#pragma once

#include <vector>
#include <string>
#include <map>

#include <boost/utility.hpp>

#include "AABB.h"
#include "Ray.h"
#include "StackRegistration.h"
#include "TinyStats.h"

class Framebuffer;
class Shader;
class ILayout;
class SpimStack;
class Viewport;
class ReferencePoints;
struct Hourglass;
class InteractionVolume;
class SimplePointcloud;
class IStackTransformationSolver;

class CreatePhantomApp : boost::noncopyable
{
public:
	CreatePhantomApp(const glm::ivec2& resolution);
	~CreatePhantomApp();
	

	void addSpimStack(const std::string& filename);
	void addSpimStack(const std::string& filename, const glm::vec3& voxelDimensions);
	void addSpimStack(SpimStack* stack);
	void subsampleAllStacks();


	void reloadShaders();
	void switchRenderMode();
	void switchBlendMode();

	void update(float dt);
	void draw();

	void resize(const glm::ivec2& newSize);
	

	/// \name Resampling
	/// \{

	void createEmptyRandomStack(const glm::ivec3& resolution);

	void startSampleStack(int stack);
	void clearSampleStack();
	void endSampleStack();

	/// \}

	//inline size_t getStacksCount() const { return stacks.size(); }
	void toggleSelectStack(int n);
	void toggleStack(int n);
	inline void toggleCurrentStack() { toggleStack(currentVolume); }
	void toggleAllStacks();

	void startStackMove();
	void endStackMove();

	/// Undos the last transform applied. This also includes movements of a stack
	void undoLastTransform();
	void moveStack(const glm::vec2& delta);
	void rotateCurrentStack(float rotY);

	void deleteSelectedStack();

	void inspectOutputImage(const glm::ivec2& cursor);

	void changeContrast(const glm::ivec2& cursor);
	void calculateHistograms();
	void increaseMaxThreshold();
	void decreaseMaxThreshold();
	void increaseMinThreshold();
	void decreaseMinThreshold();
	void autoThreshold();
	void contrastEditorApplyThresholds();
	void contrastEditorResetThresholds();


	
	void updateMouseMotion(const glm::ivec2& cursor);
	

	/// \name Views/Layout
	/// \{
	void setPerspectiveLayout(const glm::ivec2& res, const glm::ivec2& mouseCoords);
	void setTopviewLayout(const glm::ivec2& res, const glm::ivec2& mouseCoords);
	void setThreeViewLayout(const glm::ivec2& res, const glm::ivec2& mouseCoords);
	void setContrastEditorLayout(const glm::ivec2& res, const glm::ivec2& mouseCoords);
	
	/// \}


	/// \name Rendering options
	/// \{
	void increaseSliceCount();
	void decreaseSliceCount();
	void resetSliceCount();

	void rotateCamera(const glm::vec2& delta);
	void zoomCamera(float dt);
	void panCamera(const glm::vec2& delta);
	void centerCamera();
	void maximizeViews();
	
	/// \}

	inline void setCameraMoving(bool m) { cameraMoving = m; }


	void saveStackTransformations() const;
	void loadStackTransformations();
	
	void saveContrastSettings() const;
	void loadContrastSettings();

	inline void toggleGrid() { drawGrid = !drawGrid; }
	inline void toggleBboxes() { drawBboxes = !drawBboxes; }
	void toggleSlices();

	inline void setConfigPath(const std::string& p) { configPath = p; }

private:
	std::string				configPath;
	std::string				shaderPath;

	ILayout*				layout;
	std::map<std::string, ILayout*>	prevLayouts;


	// reference stack is always 0, the rest follow
	std::vector<SpimStack*>	stacks;
	
	AABB					globalBBox;

	// global contrast settings
	Threshold				globalThreshold;
	
	// normalized histograms
	std::vector<std::vector<float> > histograms;
	bool					histogramsNeedUpdate;
	float					minCursor, maxCursor;
	
	bool					cameraMoving;


	bool					drawGrid;
	bool					drawBboxes;
	bool					drawSlices;
	
	std::vector<InteractionVolume*>		interactionVolumes;
	int									currentVolume;

	// how many planes/slices to draw for the volume
	unsigned int			sliceCount;
	bool					subsampleOnCameraMove;
	
	Shader*					pointShader;
	Shader*					volumeShader;
	Shader*					sliceShader;

	Shader*					volumeDifferenceShader;
	Shader*					volumeRaycaster;
	Shader*					drawQuad;
		
	Shader*					drawPosition;

	// for GPU stack sampling
	Shader*					gpuStackSampler;
	Framebuffer*			stackSamplerTarget;

	// for contrast mapping
	Shader*					tonemapper;

	// stores undo transformations for all stacks
	struct VolumeTransform
	{
		InteractionVolume*	volume;
		glm::mat4			matrix;
	};
		
	std::vector<VolumeTransform> transformUndoChain;
		
	Framebuffer*			volumeRenderTarget;	
	Framebuffer*			rayStartTarget;

	void updateGlobalBbox();

	void drawContrastEditor(const Viewport* vp);
	void drawScoreHistory(const TinyHistory<double>& hist) const;
	void drawGroundGrid(const Viewport* vp) const;
	void drawBoundingBoxes() const;

	void drawTexturedQuad(unsigned int texture) const;
	void drawTonemappedQuad();

	void drawAxisAlignedSlices(const glm::mat4& mvp, const glm::vec3& axis, const Shader* shader) const;
	void drawAxisAlignedSlices(const Viewport* vp, const Shader* shader) const;
	void drawViewplaneSlices(const Viewport* vp, const Shader* shader) const;
	
	
	// ray tracing section
	void raytraceVolumes(const Viewport* vp) const;
	void initializeRayTargets(const Viewport* vp);


	bool useImageAutoContrast;
	float minImageContrast;
	float maxImageContrast;

	int sampleStack = -1;
		
	std::vector<glm::vec4>		stackSamples;
	void drawStackSamples() const;

	size_t						lastStackSample;
	void addStackSamples();
	

	void calculateImageContrast(const std::vector<glm::vec4>& rgbaImage);
	double calculateImageScore();

	// this is the read-back image. used for inspection etc
	std::vector<glm::vec4>	renderTargetReadback;
	bool					renderTargetReadbackCurrent = false;

	void readbackRenderTarget();

	static glm::vec3 getRandomColor(unsigned int n);

	inline bool currentVolumeValid() const { return currentVolume > -1 && currentVolume < interactionVolumes.size(); }
	
	void saveVolumeTransform(unsigned int n);
	void addInteractionVolume(InteractionVolume* v);
};