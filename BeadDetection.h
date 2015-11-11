#pragma once

#include <glm/glm.hpp>
#include <vector>


class SpimStack;

struct Hourglass
{
	// xyz = center, w = radius
	typedef glm::vec4	Circle;
	std::vector<Circle>	circles;


	void draw() const;
};



