#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>

class Shader;

class SpimPlane
{
public:
	SpimPlane(const std::string& imageFile, const std::string& registrationFile);
	~SpimPlane();

	void draw(Shader* s) const;


private:
	glm::mat4		transform;
	unsigned int	texture;


	SpimPlane(const SpimPlane&);

};