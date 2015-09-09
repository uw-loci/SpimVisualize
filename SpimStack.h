#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

struct AABB;
class Shader;

// a single stack of SPIM images
class SpimStack
{
public:
	SpimStack(const std::string& filename);
	~SpimStack();
	
	void draw() const;
	void drawVolume(Shader* s) const;


	inline const std::vector<glm::vec4>& getPoints() { return points; }
	void calculatePoints(unsigned short threshold);

	AABB&& getBBox() const;
	

private:
	unsigned int		width, height, depth;
	unsigned int		textures;

	// 3d volume texture
	unsigned int		volumeTextureId;
	mutable unsigned int	volumeList;

	unsigned short*		volume;
	
	std::vector<glm::vec4>	points;

};