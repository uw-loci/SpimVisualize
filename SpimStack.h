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
	SpimStack(const std::string& filename, unsigned int subsample=0);
	virtual ~SpimStack();
	
	void draw() const;
	void drawSlices(Shader* s, const glm::vec3& viewDir) const;

	void loadRegistration(const std::string& filename);

	// halfs the internal resolution of the dataset
	void subsample(bool updateTexture=true);
		
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


private:
	glm::vec3			dimensions;

	unsigned int		width, height, depth;
	unsigned int		textures;

	std::string			filename;

	
	// 3d volume texture
	unsigned int			volumeTextureId;

	// 2 display lists: 0->width and width->0 for quick front-to-back rendering
	mutable unsigned int	volumeList[2];

	unsigned short*			volume;
	unsigned short			minVal, maxVal;


	void loadImage(const std::string& filename);
	void loadBinary(const std::string& filename, const glm::ivec3& resolution);


	void createPlaneLists();


	void drawZPlanes(const glm::vec3& view) const;
	void drawXPlanes(const glm::vec3& view) const;
	void drawYPlanes(const glm::vec3& view) const;

	std::vector<glm::vec3> calculateVolumeNormals() const;

	//cv::Mat getImagePlane(unsigned int z) const;

};



