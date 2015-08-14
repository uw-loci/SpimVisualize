#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>

class SpimPlane
{
public:
	SpimPlane(const std::string& filename);
	~SpimPlane();

	void draw() const;


private:
	glm::mat4		transform;
	unsigned int	texture;


	SpimPlane(const SpimPlane&);

};