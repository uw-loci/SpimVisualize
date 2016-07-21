#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "InteractionVolume.h"
#include "Threshold.h"

struct AABB;
class Shader;
class Framebuffer;
class ReferencePoints;
struct Hourglass;

// a single stack of SPIM images
class SpimStack : public InteractionVolume
{
public:
	static SpimStack* load(const std::string& filename);
	static InteractionVolume* loadHeader(const std::string& filename);

	virtual ~SpimStack();
	
	void save(const std::string& filename);

	virtual void drawSlices(Shader* s, const glm::vec3& viewDir) const;
	virtual void drawZSlices() const;

	void loadRegistration(const std::string& filename);
	void loadFijiRegistration(const std::string& path);

	
	// halfs the internal resolution of the dataset
	virtual void subsample(bool updateTexture=true) = 0;

	// sets the contents for the whole stack. It also erases the data and resizes it
	virtual void setContent(const glm::ivec3& resolution, const void* data) = 0;
	
	// sets a single sample at a specific location
	inline void setSample(const glm::ivec3& pos, float value) { setSample(getIndex(pos.x, pos.y, pos.z), value);	}
	virtual void setSample(const size_t index, float value) = 0;

	inline float getSample(const glm::vec3& worldCoords) const { return getSample(getStackVoxelCoords(worldCoords)); }
	float getSample(const glm::ivec3& stackCoords) const;
	
	// set all samples of a single z plane
	void setPlaneSamples(const std::vector<float>& values, size_t zplane);

	// updates the internal stats and the texture
	virtual void update();

	// reslices this stack along z
	virtual void reslice(unsigned int minZ, unsigned int maxZ) = 0;
			

	virtual size_t getBytesPerVoxel() const = 0;

	// extracts the points in world coords. The w coordinate contains the point's value
	std::vector<glm::vec4> extractTransformedPoints() const;
	// extracts the points in world space and clip them against the other's transformed bounding box. The w coordinate contains the point's value
	std::vector<glm::vec4> extractTransformedPoints(const SpimStack* clip, const Threshold& t) const;

	void extractTransformedFeaturePoints(const Threshold& t, ReferencePoints& result) const;

	virtual AABB getTransformedBBox() const;
	glm::vec3 getCentroid() const;

	// returns the world position of the given coordinate
	glm::vec3 getWorldPosition(const glm::ivec3& coordinate) const;

	// returns the world position of the given index
	glm::vec3 getWorldPosition(const unsigned int index) const;

	// returns the internal stack voxel coordinates for a given world coordinate
	glm::ivec3 getStackVoxelCoords(const glm::vec4& worldCoords) const;
	inline glm::ivec3 getStackVoxelCoords(const glm::vec3& worldCoords) const { return getStackVoxelCoords(glm::vec4(worldCoords, 1.f)); }
	
	inline size_t getPlanePixelCount() const { return width*height; }

	inline const unsigned int getTexture() const { return volumeTextureId; }
	

	std::vector<size_t> calculateHistogram(const Threshold& t) const;
	inline Threshold& getThreshold() { return threshold;  }
	const Threshold& updateThreshold();
	void autoThreshold();

	glm::ivec3 getStackCoords(size_t index) const;

	inline glm::ivec3 getResolution() const { return glm::ivec3(width, height, depth); }
	inline const glm::vec3& getVoxelDimensions() const { return dimensions; }
	void setVoxelDimensions(const glm::vec3& dimensions);

	inline const std::string& getFilename() const { return filename;  }
	

	/// \name Filtering methods
	/// \{

	
	virtual void addSaltPepperNoise(float satl, float pepper, float amount);
	virtual void applyGaussianBlur(float sigma, int radius);
	virtual void applyMedianFilter(const glm::ivec3& window);


	/// \}

	inline unsigned int getWidth() const { return width; }
	inline unsigned int getHeight() const { return height; }
	inline unsigned int getDepth() const { return depth; }
	inline size_t getVoxelCount() const { return width*height*depth; }

protected:
	SpimStack();
	
	glm::vec3			dimensions;

	unsigned int		width, height, depth;
	unsigned int		textures;

	std::string			filename;

	
	// 3d volume texture
	unsigned int			volumeTextureId;

	// 2 display lists: 0->width and width->0 for quick front-to-back rendering
	mutable unsigned int	volumeList[2];
	
	// per-volume thresholds
	Threshold				threshold;


	virtual void updateStats();
	virtual void updateTexture() = 0;


	virtual float getValue(size_t index) const = 0;
	virtual float getRelativeValue(size_t index) const = 0;
	
	virtual void getValues(float* data) const;
	virtual void setValues(const float* data);

	virtual void loadImage(const std::string& filename) = 0;
	virtual void loadBinary(const std::string& filename, const glm::ivec3& resolution) = 0;

	virtual void saveBinary(const std::string& filename) = 0;
	virtual void saveImage(const std::string& filename) = 0;

	inline size_t getIndex(unsigned int x, unsigned int y, unsigned int z) const { return x + y*width + z*width*height; }
	inline size_t getIndex(const glm::ivec3& coords) const { return getIndex(coords.x, coords.y, coords.z); }
	

	void createPlaneLists();


	void drawZPlanes(const glm::vec3& view) const;
	void drawXPlanes(const glm::vec3& view) const;
	void drawYPlanes(const glm::vec3& view) const;

	std::vector<glm::vec3> calculateVolumeNormals() const;



};

class SpimStackU16 : public SpimStack
{
public:
	SpimStackU16();
	~SpimStackU16();

	virtual void subsample(bool updateTexture = true);
	virtual void setContent(const glm::ivec3& resolution, const void* data);
	virtual void setSample(const size_t index, float value);

	virtual void reslice(unsigned int minZ, unsigned int maxZ);

	virtual size_t getBytesPerVoxel() const { return 2; }

private:
	unsigned short*			volume;

	SpimStackU16(const SpimStackU16& cp);

	virtual void updateTexture();

	virtual void loadImage(const std::string& filename);
	virtual void loadBinary(const std::string& filename, const glm::ivec3& resolution);
	

	virtual void saveBinary(const std::string& filename);
	virtual void saveImage(const std::string& filename);
	
	inline float getValue(size_t index) const
	{
		assert(index < width*height*depth);
		return (float)volume[index];
	}

	inline float getRelativeValue(size_t index) const
	{
		return getValue(index) / std::numeric_limits<unsigned short>::max();
	}
};

class SpimStackU8 : public SpimStack
{
public:
	SpimStackU8(); 
	~SpimStackU8();

	virtual void subsample(bool updateTexture = true);
	virtual void setContent(const glm::ivec3& resolution, const void* data);
	virtual void setSample(const size_t index, float value);

	virtual void reslice(unsigned int minZ, unsigned int maxZ);

	virtual size_t getBytesPerVoxel() const { return 1; }

private:
	unsigned char*			volume;

	SpimStackU8(const SpimStackU8&);
	
	virtual void updateTexture();

	virtual void loadImage(const std::string& filename);
	virtual void loadBinary(const std::string& filename, const glm::ivec3& resolution);

	virtual void saveBinary(const std::string& filename);
	virtual void saveImage(const std::string& filename);

	inline float getValue(size_t index) const
	{
		//assert(index < width*heigth*depth);
		return (float)volume[index];
	}

	inline float getRelativeValue(size_t index) const
	{
		return getValue(index) / 255;
	}
};
