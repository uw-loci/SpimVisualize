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

class SpimRegistrationApp : boost::noncopyable
{
public:
	SpimRegistrationApp(const glm::ivec2& resolution);
	~SpimRegistrationApp();
	

	void addSpimStack(const std::string& filename);
	void subsampleAllStacks();


	void reloadShaders();

	void update(float dt);
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
	inline void toggleSlices() { drawSlices = !drawSlices; }
	

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
	
	// for contrast mapping
	Shader*					tonemapper;

	

	// stores undo transformations for all stacks
	struct StackTransform
	{
		SpimStack*		stack;
		glm::mat4		matrix;
	};
		
	std::vector<StackTransform> transformUndoChain;
		
	Framebuffer*			volumeRenderTarget;
	
	




	void updateGlobalBbox();

	void drawContrastEditor(const Viewport* vp);
	void drawScene(const Viewport* vp);
	
	void drawGroundGrid(const Viewport* vp) const;
	void drawBoundingBoxes() const;

	void drawTexturedQuad(unsigned int texture) const;
	void drawTonemappedQuad(unsigned int texture) const;

	void drawAxisAlignedSlices(const glm::mat4& mvp, const glm::vec3& axis, const Shader* shader) const;
	void drawAxisAlignedSlices(const Viewport* vp, const Shader* shader) const;
	void drawViewplaneSlices(const Viewport* vp, const Shader* shader) const;
	void raycastVolumes(const Viewport* vp, const Shader* shader) const;

	
	// auto-stack alignment
	unsigned int			lastSamplesPass;
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

	bool				occlusionQueryMask[4];
	unsigned int		occlusionQueries[4];
	unsigned int		singleOcclusionQuery;


	void createCandidateTransforms();
	void selectAndApplyBestTransform();


	static glm::vec3 getRandomColor(int n);

	inline bool currentStackValid() const { return currentStack > -1 && currentStack < stacks.size();  }

	void saveStackTransform(unsigned int n);
	

};