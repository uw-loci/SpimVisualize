#pragma once

#include <vector>
#include <string>
#include <map>

#include <boost/utility.hpp>

#include "AABB.h"
#include "Config.h"
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
class IWidget;

class SpimRegistrationApp : boost::noncopyable
{
public:
	SpimRegistrationApp(const glm::ivec2& resolution);
	~SpimRegistrationApp();

	void addSpimStack(SpimStack* stack);
	void addSpimStack(const std::string& filename);
	void addSpimStack(const std::string& filename, const glm::vec3& voxelScale);
	void subsampleAllStacks();


	void addPointcloud(const std::string& filename);
	void addPhantom(const std::string& stackFilename, const std::string& referenceTransform, const glm::vec3& voxelDimensions, bool fijiTransform = false);

	//void loadPrevSolutionSpace(const std::string& filename);

	// presentation mode -- rotate camera ... 
	void toggleRotateCamera();

	void reloadShaders();
	void reloadConfig();

	void update(float dt);
	void draw();

	void resize(const glm::ivec2& newSize);

	//inline size_t getStacksCount() const { return stacks.size(); }
	void toggleSelectVolume(int n);
	void toggleVolume(int n);
	inline void toggleCurrentVolume() { toggleVolume(currentVolume); }
	void toggleAllVolumes();
	void deselectAll();
	void toggleVolumeLock(int n);
	inline void toggleCurrentVolumeLock() {toggleVolumeLock(currentVolume);}
	void unlockAllVolumes();
	
	void startStackMove(const glm::ivec2& mouse);
	void updateStackMove(const glm::ivec2& mouse);
	void updateStackMove(const glm::ivec2& mouse, float valueStep);
	void endStackMove(const glm::ivec2& mouse);

	void setWidgetMode(const std::string& mode);


	/// Undos the last transform applied. This also includes movements of a stack
	void undoLastTransform();
	void moveCurrentStack(const glm::vec2& delta);
	void rotateCurrentVolume(float rotY);

	void setWidgetType(const std::string& control);

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


	void applyGaussFilterToCurrentStack();

	void centerGrid();

	/// \name Pointclods
	/// \{

	void bakeSelectedTransform();
	void saveCurrentPointcloud();

	/// \}

	/// \name Phantom creation
	/// \{ 


	void createEmptyRandomStack();
	void createEmptyRandomStack(const glm::ivec3& resolution, const glm::vec3& voxelDimensions);
	void createEmptyFullSizedStack();

	void startSampleStack(int stack);
	void clearSampleStack();
	void endSampleStack();

	inline void resampleSelectedStack() { startSampleStack(currentVolume); }



	/// \}

	void createFakeBeads(unsigned int beadCount) ;

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
	
	// Begins the auto alignment process through all stacks, selected randomly
	void beginMultiAutoAlign();
	
	// runs one iteration of the alignment
	void runAlignmentOnce();
	
	
	/// Selects the currently active solver
	void selectSolver(const std::string& solver);
	
	void toggleHistory();
	void clearHistory();

	/// \}

	void updateMouseMotion(const glm::ivec2& cursor);
	

	/// \name Views/Layout
	/// \{
	void setLayout(const std::string& name, const glm::ivec2& res, const glm::ivec2& mouseCoords);

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

	
	void saveStackTransformations();
	void loadStackTransformations();
	void clearStackTransformations();
	
	inline void toggleGrid() { drawGrid = !drawGrid; }
	inline void toggleBboxes() { drawBboxes = !drawBboxes; }
	void toggleSlices();
	inline void togglePhantoms() { drawPhantoms = !drawPhantoms;  }

	inline void toggleSolutionParameterSpace() { drawSolutionSpace = !drawSolutionSpace; }
	inline void clearSolutionParameterSpace() { solutionParameterSpace.clear(); }

	void alignPhantoms();

	inline void loadConfig(const std::string& file) { config.load(file); }
	inline void saveConfig(const std::string& file) const { config.save(file); }


	void resliceStack(unsigned int stack);


	inline void setShaderPath(const std::string& sp) { shaderPath = sp; }

private:
	Config					config;
	std::string				shaderPath;

	ILayout*				layout;
	std::map<std::string, ILayout*>	prevLayouts;



	std::vector<SpimStack*>	stacks;

	AABB					globalBBox;
	
	// test interaction
	std::vector<Ray>		rays;

	// normalized histograms
	std::vector<std::vector<float> > histograms;
	bool					histogramsNeedUpdate;
	float					minCursor, maxCursor;
	
	std::vector<SimplePointcloud*>	pointclouds;


	bool					cameraAutoRotate;
	bool					cameraMoving;


	glm::vec3				gridOffset;
	bool					drawGrid;
	bool					drawBboxes;
	bool					drawSlices;
	bool					drawHistory;
	bool					drawPhantoms;


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
	Shader*					pointSpriteShader;

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


	// to better change the selected volume
	IWidget*				controlWidget;

	struct Phantom
	{
		std::string		stackFile;
		AABB			bbox;
		glm::mat4		transform;
		glm::mat4		originalTransform;
	};
	std::vector<Phantom>	phantoms;

	void updateGlobalBbox();

	void drawContrastEditor(const Viewport* vp);
	void drawScoreHistory(const TinyHistory<double>& hist) const;
	void drawGroundGrid(const Viewport* vp) const;
	void drawBoundingBoxes() const;
	void drawPhantomBoxes() const;
	void drawStackSamples() const;

	void drawTexturedQuad(unsigned int texture) const;
	void drawTonemappedQuad();

	void drawAxisAlignedSlices(const glm::mat4& mvp, const glm::vec3& axis, const Shader* shader) const;
	void drawAxisAlignedSlices(const Viewport* vp, const Shader* shader) const;
	void drawViewplaneSlices(const Viewport* vp, const Shader* shader) const;
	
	
	void reloadVolumeShader();

	// ray tracing section
	void raytraceVolumes(const Viewport* vp) const;
	void initializeRayTargets(const Viewport* vp);

	void drawPointclouds(const Viewport* vp);
	void drawPointSpritePointclouds(const Viewport* vp);

	void drawRays(const Viewport* vp);
	void drawSolutionParameterSpace(const Viewport* vp) const;


	bool useImageAutoContrast;
	float minImageContrast;
	float maxImageContrast;

	void calculateImageContrast(const std::vector<glm::vec4>& rgbaImage);
	double calculateImageScore();

	// auto-stack alignment
	bool				runAlignment;
	bool				runAlignmentOnlyOncePlease;
	bool				multiAlign;

	void selectAndApplyBestSolution();

	// this is the read-back image. used for inspection etc
	std::vector<glm::vec4>	renderTargetReadback;
	bool					renderTargetReadbackCurrent;

	void readbackRenderTarget();


	// this is about resampling
	int sampleStack = -1;

	std::vector<glm::vec4>		stackSamples;
	
	size_t						lastStackSample;
	void addStackSamples();
	void addMultiStackSamples();

	// for GPU stack sampling
	// single stack sampler
	Shader*					gpuStackSampler;

	// multistack sampler
	Shader*					gpuMultiStackSampler;
	Framebuffer*			stackSamplerTarget;



	unsigned int	pointSpriteTexture;
	void createPointSpriteTexture();


	// this one will override runAlignment and always calculate the score 
	bool				calculateScore;
	TinyHistory<double>	scoreHistory;


	IStackTransformationSolver*		solver;
	
	// just for debugging
	std::vector<glm::vec4>			solutionParameterSpace;
	bool							drawSolutionSpace;

	static glm::vec3 getRandomColor(unsigned int n);

	inline bool currentVolumeValid() const { return currentVolume > -1 && currentVolume < (int)interactionVolumes.size(); }

	void saveVolumeTransform(unsigned int n);
	void addInteractionVolume(InteractionVolume* v);
	
	unsigned int saveCounter;

	void inspectPointclouds(const Ray& ray);
};