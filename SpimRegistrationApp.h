#pragma once

#include <vector>
#include <string>
#include <map>

#include <boost/utility.hpp>

#include "AABB.h"
#include "StackRegistration.h"

class Framebuffer;
class Shader;
class ILayout;
class SpimStack;
class Viewport;
class ReferencePoints;
struct Hourglass;
class InteractionVolume;
class SimplePointcloud;

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
	
	void startStackMove();
	void endStackMove();


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


	void toggleAllStacks();


	
	
	void beginAutoAlign();
	void endAutoAlign();
	void undoLastTransform();

	void updateMouseMotion(const glm::ivec2& cursor);
	

	void setPerspectiveLayout(const glm::ivec2& res, const glm::ivec2& mouseCoords);
	void setTopviewLayout(const glm::ivec2& res, const glm::ivec2& mouseCoords);
	void setThreeViewLayout(const glm::ivec2& res, const glm::ivec2& mouseCoords);
	void setContrastEditorLayout(const glm::ivec2& res, const glm::ivec2& mouseCoords);
	

	void increaseSliceCount();
	void decreaseSliceCount();
	void resetSliceCount();

	void rotateCamera(const glm::vec2& delta);
	void zoomCamera(float dt);
	void panCamera(const glm::vec2& delta);
	void centerCamera();
	void maximizeViews();


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

	// normalized histograms
	std::vector<std::vector<float> > histograms;
	bool					histogramsNeedUpdate;
	float					minCursor, maxCursor;
	
	std::vector<SimplePointcloud*>	pointclouds;

	bool					cameraMoving;


	bool					drawGrid;
	bool					drawBboxes;
	bool					drawSlices;
	bool					drawRegistrationPoints;


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
		
	// for contrast mapping
	Shader*					tonemapper;

	enum RenderMode
	{
		RENDER_VIEWPLANE_SLICES,
		RENDER_ALIGN

	}						renderMode;

	enum BlendMode
	{
		BLEND_ADD,
		BLEND_MAX

	}						blendMode;

	// stores undo transformations for all stacks
	struct VolumeTransform
	{
		InteractionVolume*	volume;
		glm::mat4			matrix;
	};
		
	std::vector<VolumeTransform> transformUndoChain;
		
	Framebuffer*			volumeRenderTarget;	

	void updateGlobalBbox();

	void drawContrastEditor(const Viewport* vp);
	
	void drawGroundGrid(const Viewport* vp) const;
	void drawBoundingBoxes() const;

	void drawTexturedQuad(unsigned int texture) const;
	void drawTonemappedQuad(Framebuffer* fbo) const;

	void drawAxisAlignedSlices(const glm::mat4& mvp, const glm::vec3& axis, const Shader* shader) const;
	void drawAxisAlignedSlices(const Viewport* vp, const Shader* shader) const;
	void drawViewplaneSlices(const Viewport* vp, const Shader* shader) const;
	void raycastVolumes(const Viewport* vp, const Shader* shader) const;

	void drawPointclouds(const Viewport* vp);
	
	bool useImageAutoContrast;
	float minImageContrast;
	float maxImageContrast;

	void calculateImageContrast(const std::vector<glm::vec4>& rgbaImage);


	// auto-stack alignment
	unsigned long long		lastSamplesPass;
	glm::mat4				lastPassMatrix;
		
	bool					runAlignment;

	struct OcclusionQueryResult
	{
		glm::mat4			matrix;
		unsigned long long	result[4];
		bool				ready;

		inline unsigned long long getScore() const
		{
			return result[0] + result[1] + result[2] + result[3];
		}

		inline bool operator < (const OcclusionQueryResult& rhs) const
		{
			return getScore() < rhs.getScore();
		}
	};

	std::vector<glm::mat4>				candidateTransforms;
	std::vector<OcclusionQueryResult>	occlusionQueryResults;
	OcclusionQueryResult				currentResult;

	bool				useOcclusionQuery;
	unsigned int		singleOcclusionQuery;

	void createCandidateTransforms();
	void selectAndApplyBestTransform();

	double calculateScore( Framebuffer* fbo) const;


	static glm::vec3 getRandomColor(unsigned int n);

	inline bool currentVolumeValid() const { return currentVolume > -1 && currentVolume < interactionVolumes.size(); }
	
	void saveVolumeTransform(unsigned int n);
	void addInteractionVolume(InteractionVolume* v);
	

};