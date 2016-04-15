#pragma once

#include <vector>
#include <glm/glm.hpp>

#include "InteractionVolume.h"

class Shader;

class SimplePointcloud : public InteractionVolume
{
public:
	SimplePointcloud(const std::string& filename, const glm::mat4& transform = glm::mat4(1.f));
	~SimplePointcloud();

	void draw() const;

	// very bare-bones rendering. All the shader setup (including matrices) must be done externally. No colors, only position 
	void submitVertices() const;

	inline size_t getPointcount() const { return pointCount; }
	inline const std::vector<glm::vec3> getPoints() const { return vertices; }

	inline const std::string& getFilename() const { return filename;  }

	// transforms all points
	void bakeTransform() ;

	void saveBin(const std::string& filename);


private:	
	size_t						pointCount;

	std::string					filename;

	// position and color opengl buffer
	unsigned int				vertexBuffers[2];
		
	typedef std::vector<glm::vec3> PointCloud;

	PointCloud					vertices, colors;

	void loadTxt(const std::string& filename);
	void loadBin(const std::string& filename);
	


	void updateBuffers();
};


