#pragma once

#include <glm/glm.hpp>
#include <vector>


class SpimStack;

struct Hourglass
{
	// the XY coords of the axis = the center axis of the hourglass (always orthogonal to the XY plane) in pixel coords
	glm::vec2		centerAxis;

	// a list of z coords an radii of the circles making up the hourglass in pixel dimensions
	std::vector<glm::vec2>	circles;


	void draw() const;
};



