#pragma once

#include <glm/glm.hpp>
#include <vector>

struct AABB;

struct Ray
{
	glm::vec3	origin;
	glm::vec3	direction;
		

	/// casts a ray in the given frustum from the near to the far plane.
	/** The direction will not be normalized
		@param mvp camera modelviewprojection matrix
		@param coords mouse coords from [-1..1] on the near plane.
	*/
	void createFromFrustum(const glm::mat4& mvp, const glm::vec2& coords);

	bool intersectsAABB(const AABB& bbox) const;
	bool intersectsAABB(const AABB& bbox, const glm::mat4& boxTransform) const;

	void transform(const glm::mat4& transform);

	size_t getClosestPoint(const std::vector<glm::vec3>& points, float& distSqrd) const;
	size_t getClosestPoint(const std::vector<glm::vec3>& points, const glm::mat4& pointTransform, float& distSqrd) const;

};

