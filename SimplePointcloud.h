#pragma once

#include <vector>
#include <glm/glm.hpp>

#include "InteractionVolume.h"

class Shader;

class SimplePointcloud : public InteractionVolume
{
public:
	SimplePointcloud(const std::string& filename);
	~SimplePointcloud();

	void draw() const;
	void draw(Shader* sh) const;
	
	inline size_t getPointcount() const { return pointCount; }

	static void resaveAsBin(const std::string& filename);

private:	
	size_t						pointCount;

	// position and color opengl buffer
	unsigned int				vertexBuffers[3];
	
	
	typedef std::vector<glm::vec3> PointCloud;

	static void loadTxt(const std::string& filename, PointCloud& vertices, PointCloud& normals, PointCloud& colors);
	static void loadBin(const std::string& filename, PointCloud& vertices, PointCloud& normals, PointCloud& colors);
	static void saveBin(const std::string& filename, const PointCloud& vertices, const PointCloud& normals, const PointCloud& colors);
};


