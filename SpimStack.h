#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

struct AABB;

// a single stack of SPIM images
class SpimStack
{
public:
	SpimStack(const std::string& filename);
	~SpimStack();
	
	void draw() const;


	inline const std::vector<glm::vec4>& getPoints() { return points; }
	void calculatePoints(unsigned short threshold);

	AABB&& getBBox() const;
	

private:
	unsigned int		width, height, depth;
	unsigned int		textures;

	
	unsigned short*		volume;
	
	std::vector<glm::vec4>	points;

};