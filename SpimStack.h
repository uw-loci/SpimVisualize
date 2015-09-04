#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

// a single stack of SPIM images
class SpimStack
{
public:
	SpimStack(const std::string& filename);
	~SpimStack();
	
	void draw() const;


	std::vector<glm::vec4> getPointcloud(unsigned short threshold) const;
	
private:
	unsigned int		width, height, depth;
	unsigned int		textures;

	
	unsigned short*		volume;

};