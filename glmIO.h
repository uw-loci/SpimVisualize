#pragma once

#include <glm/glm.hpp>
#include <ostream>

static inline std::ostream& operator << (std::ostream& os, const glm::vec3& v)
{
	return os << "(" << v.x << ", " << v.y << ", " << v.z << ")";
}

static inline std::ostream& operator << (std::ostream& os, const glm::vec4& v)
{
	return os << "(" << v.x << ", " << v.y << ", " << v.z << ", " << v.z << ")";
}


static inline std::ostream& operator << (std::ostream& os, const glm::mat4& m)
{
	return os << "[" << m[0] << "\n" << m[1] << "\n" << m[2] << "\n" << m[3] << "]";
}