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

class SpimRegistrationApp : boost::noncopyable
{
public:
	SpimRegistrationApp(const glm::ivec2& resolution);
	~SpimRegistrationApp();
	

	void addSpimStack(const std::string& filename);
	void subsampleAllStacks();


	void addPointcloud(const std::string& filename);


	void reloadShaders();
	void switchRenderMode();
	void switchBlendMode();

	void update(float dt);
	void draw();

	void resize(const glm::ivec2& newSize);
	
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

	void inspectOutputImage(const glm::ivec2& cursor);

	void changeContrast(const glm::ivec2& cursor);
	void calculateHistograms();
	void increaseMaxThreshold();
	void decreaseMaxThreshold();
	void increaseMinThreshold();
	void decreaseMinThreshold();
	void autoThreshold();
	void contrastEditorApplyThresholds();


	

	inline void clearRays() { rays.clear(); }
	
	/// \name Alignment
	/// \{
	/// Begins the auto alignment process with the currently selected solver
	/**	Each frame the solver will try a different solution and record the score.
		The alignment process can be stopped by calling @endAutoAlign. At that point 
		the best solution is selected and applied. 
	*/
	void beginAutoAlign();
	void endAutoAlign();	
	/// Selects the currently active solver
	void selectSolver(const std::string& solver);
	

	void clearHistory();

	/// \}

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
	
	ILayout*				layout;
	std::map<std::string, ILayout*>	prevLayouts;



	std::vector<SpimStack*>	stacks;

	AABB					globalBBox;

	// global contrast settings
	Threshold				globalThreshold;


	// test interaction
	std::vector<Ray>		rays;

	// normalized histograms
	std::vector<std::vector<float> > histograms;
	bool					histogramsNeedUpdate;
	float					minCursor, maxCursor;
	
	std::vector<SimplePointcloud*>	pointclouds;

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

	Shader*					volumeRaycaster;
	Shader*					drawQuad;
	Shader*					volumeDifferenceShader;		
	Shader*					drawPosition;

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

	void drawPointclouds(const Viewport* vp);
	
	void drawRays(const Viewport* vp);



	bool useImageAutoContrast;
	float minImageContrast;
	float maxImageContrast;

	void calculateImageContrast(const std::vector<glm::vec4>& rgbaImage);
	double calculateImageScore();

	// auto-stack alignment
	bool				runAlignment;

	// this is the read-back image. used for inspection etc
	std::vector<glm::vec4>	renderTargetReadback;
	bool					renderTargetReadbackCurrent;

	void readbackRenderTarget();


	// this one will override runAlignment and always calculate the score 
	bool				calculateScore;
	TinyHistory<double>	scoreHistory;


	IStackTransformationSolver*		solver;
	
	static glm::vec3 getRandomColor(unsigned int n);

	inline bool currentVolumeValid() const { return currentVolume > -1 && currentVolume < (int)interactionVolumes.size(); }
	
	void saveVolumeTransform(unsigned int n);
	void addInteractionVolume(InteractionVolume* v);
	

	void inspectPointclouds(const Ray& ray);
};