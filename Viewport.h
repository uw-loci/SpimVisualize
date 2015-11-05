#pragma once

#include <glm/glm.hpp>

class ICamera;

struct Viewport
{
	// window coordinates
	glm::ivec2		position, size;

	// projection and view matrices
	glm::mat4		proj, view;

	glm::vec3		color;

	enum ViewportName { ORTHO_X = 0, ORTHO_Y, ORTHO_Z, PERSPECTIVE = 3 } name;

	bool			highlighted;
	float			orthoZoomLevel;
	glm::vec2		orthoZenter;

	ICamera*		camera;

	inline bool isInside(const glm::ivec2& cursor) const
	{
		using namespace glm;
		const ivec2 rc = cursor - position;
		return all(greaterThanEqual(rc, ivec2(0))) && all(lessThanEqual(rc, size));
	}


	void orthoZoom(float z);

	void setup() const;

	inline glm::vec3 getCameraPosition() const
	{
		return glm::vec3(glm::inverse(view) * glm::vec4(0.f, 0.f, 0.f, 1.f));
	}
};
