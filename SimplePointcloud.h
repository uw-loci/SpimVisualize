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

private:	
	size_t						pointCount;

	// position and color opengl buffer
	unsigned int				vertexBuffers[3];
	
	void loadTxt(const std::string& filename);
	void loadBin(const std::string& filename);
	void saveBin(const std::string& filename);
};


