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

	void loadRegistration(const std::string& filename);

	std::vector<glm::vec4> calculatePoints() const;
	const std::vector<glm::vec3>& calculateRegistrationPoints(float threshold) ;


	AABB&& getBBox() const;

	void setRotation(float angle);

	inline const glm::mat4& getTransform() const { return transform; }
	inline const std::vector<glm::vec3>& getRegistrationPoints() const { return registrationPoints;  }

private:
	unsigned int		width, height, depth;
	unsigned int		textures;

	// 3d volume texture
	unsigned int			volumeTextureId;
	mutable unsigned int	volumeList;

	unsigned short*			volume;
	unsigned short			minVal, maxVal;


	glm::mat4				transform;

	std::vector<glm::vec3>	registrationPoints;
};



