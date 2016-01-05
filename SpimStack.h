#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "InteractionVolume.h"

struct AABB;
class Shader;
class ReferencePoints;
struct Threshold;
struct Hourglass;

// a single stack of SPIM images
class SpimStack : public InteractionVolume
{
public:
	SpimStack();
	virtual ~SpimStack();
	
	void load(const std::string& filename);

	virtual void drawSlices(Shader* s, const glm::vec3& viewDir) const;

	void loadRegistration(const std::string& filename);

	// halfs the internal resolution of the dataset
	virtual void subsample(bool updateTexture=true) = 0;
		
	// extracts the points in world coords. The w coordinate contains the point's value
	std::vector<glm::vec4> extractTransformedPoints() const;
	// extracts the points in world space and clip them against the other's transformed bounding box. The w coordinate contains the point's value
	std::vector<glm::vec4> extractTransformedPoints(const SpimStack* clip, const Threshold& t) const;

	void extractTransformedFeaturePoints(const Threshold& t, ReferencePoints& result) const;

	virtual AABB getTransformedBBox() const;
	glm::vec3 getCentroid() const;

	inline const unsigned int getTexture() const { return volumeTextureId; }
	
	Threshold getLimits() const;
	std::vector<size_t> calculateHistogram(const Threshold& t) const;



	inline const std::string& getFilename() const { return filename;  }


protected:
	glm::vec3			dimensions;

	unsigned int		width, height, depth;
	unsigned int		textures;

	std::string			filename;

	
	// 3d volume texture
	unsigned int			volumeTextureId;

	// 2 display lists: 0->width and width->0 for quick front-to-back rendering
	mutable unsigned int	volumeList[2];
	
	double					minVal, maxVal;
	

	virtual void updateStats();
	virtual void updateTexture() = 0;


	virtual double getValue(size_t index) const = 0;
	virtual double getRelativeValue(size_t index) const = 0;

	virtual void loadImage(const std::string& filename) = 0;
	virtual void loadBinary(const std::string& filename, const glm::ivec3& resolution) = 0;

	inline size_t getIndex(unsigned int x, unsigned int y, unsigned int z) const { return x + y*width + z*width*height; }
	
	void createPlaneLists();


	void drawZPlanes(const glm::vec3& view) const;
	void drawXPlanes(const glm::vec3& view) const;
	void drawYPlanes(const glm::vec3& view) const;

	std::vector<glm::vec3> calculateVolumeNormals() const;

};

class SpimStackU16 : public SpimStack
{
public:
	~SpimStackU16();

	virtual void subsample(bool updateTexture = true);

private:
	unsigned short*			volume;

	virtual void updateTexture();

	virtual void loadImage(const std::string& filename);
	virtual void loadBinary(const std::string& filename, const glm::ivec3& resolution);

	
	inline double getValue(size_t index) const
	{
		//assert(index < width*heigth*depth);
		return (float)volume[index];
	}

	inline double getRelativeValue(size_t index) const
	{
		return getValue(index) / std::numeric_limits<unsigned short>::max();
	}
};

class SpimStackU8 : public SpimStack
{
public:
	~SpimStackU8();

	virtual void subsample(bool updateTexture = true);

private:
	unsigned char*			volume;

	virtual void updateTexture();

	virtual void loadImage(const std::string& filename);
	virtual void loadBinary(const std::string& filename, const glm::ivec3& resolution);


	inline double getValue(size_t index) const
	{
		//assert(index < width*heigth*depth);
		return (float)volume[index];
	}

	inline double getRelativeValue(size_t index) const
	{
		return getValue(index) / 255;
	}
};

