#include "Ray.h"
#include "AABB.h"

using namespace glm;

void Ray::createFromFrustum(const mat4& mvp, const vec2& coords)
{
	mat4 imvp = inverse(mvp);

	vec4 nc = vec4(coords, -1.f, 1.f);
	vec4 fc = vec4(coords, 1.f, 1.f);

	nc = imvp * nc;
	nc /= nc.w;

	fc = imvp * fc;
	fc /= fc.w;


	origin = vec3(nc);
	direction = vec3(fc) - origin;

}

void Ray::transform(const glm::mat4& t)
{
	vec4 o = t * vec4(origin, 1.f);
	vec4 d = t * vec4(direction, 0.f);
	
	origin = vec3(o);
	direction = vec3(d);
}

bool Ray::intersectsAABB(const AABB& bbox) const
{		
	return bbox.isIntersectedByRay(origin, direction);
}

bool Ray::intersectsAABB(const AABB& bbox, const glm::mat4& boxTransform) const
{
	// transform ray into box space
	mat4 invX = inverse(boxTransform);
	vec4 o = invX * vec4(origin, 1.f);
	vec4 d = invX * vec4(direction, 0.f);

	return bbox.isIntersectedByRay(vec3(o), normalize(vec3(d)));
}

size_t Ray::getClosestPoint(const std::vector<vec3>& points, float& minDistance) const
{
	size_t minIndex = 0;
	minDistance = std::numeric_limits<float>::max();

	float u = length(direction);

	for (size_t i = 0; i < points.size(); ++i)
	{
		vec3 pq = points[i] - origin;
		
		float dist = length(cross(pq, direction)) / u;

		if (dist < minDistance)
		{
			minDistance = dist;
			minIndex = i;
		}

	}
	
	return minIndex;
}

size_t Ray::getClosestPoint(const std::vector<glm::vec3>& points, const glm::mat4& pointTransform, float& distSqrd) const
{
	// transform ray into box space
	mat4 invX = inverse(pointTransform);
	vec4 o = invX * vec4(origin, 1.f);
	vec4 d = invX * vec4(direction, 0.f);
	
	Ray r2;
	r2.origin = vec3(o);
	r2.direction = normalize(vec3(d));

	return r2.getClosestPoint(points, distSqrd);
}