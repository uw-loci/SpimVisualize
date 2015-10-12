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
	void drawSlices(Shader* s, const glm::vec3& viewDir) const;

	void loadRegistration(const std::string& filename);

	std::vector<glm::vec4> calculatePoints() const;
	const std::vector<glm::vec3>& calculateRegistrationPoints(float threshold) ;


	AABB&& getBBox() const;

	void setRotation(float angle);

	void move(const glm::vec3& delta);
	void rotate(float d);

	inline const glm::mat4& getTransform() const { return transform; }
	inline const std::vector<glm::vec3>& getRegistrationPoints() const { return registrationPoints;  }

	glm::vec3 getCentroid() const;

	inline const unsigned int getTexture() const { return volumeTextureId; }

	inline bool isEnabled() const { return enabled; }
	inline void toggle() { enabled = !enabled;  }




private:
	unsigned int		width, height, depth;
	unsigned int		textures;

	bool				enabled;

	// 3d volume texture
	unsigned int			volumeTextureId;

	// 2 display lists: 0->width and width->0 for quick front-to-back rendering
	mutable unsigned int	volumeList[2];

	unsigned short*			volume;
	unsigned short			minVal, maxVal;


	glm::mat4				transform;

	std::vector<glm::vec3>	registrationPoints;



	void createPlaneLists();


	void drawZPlanes(const glm::vec3& view) const;
	void drawXPlanes(const glm::vec3& view) const;
	void drawYPlanes(const glm::vec3& view) const;


};



