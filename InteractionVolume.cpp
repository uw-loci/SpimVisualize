#include "InteractionVolume.h"
#include "AABB.h"

#include <fstream>
#include <iostream>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/io.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform2.hpp>



InteractionVolume::InteractionVolume() : transform(1.f), enabled(true)
{
}

AABB InteractionVolume::getTransformedBBox() const
{
	using namespace glm;
	using namespace std;

	vector<vec3> verts = getBBox().getVertices();

	AABB bbox;
	for (size_t i = 0; i < verts.size(); ++i)
	{
		vec4 v = transform * vec4(verts[i], 1.f);
		bbox.extend(vec3(v));
	}

	return std::move(bbox);
}

void InteractionVolume::saveTransform(const std::string& filename) const
{
	std::ofstream file(filename);

	if (file.is_open())
	{

		const float* m = glm::value_ptr(transform);

		for (int i = 0; i < 16; ++i)
			file << m[i] << std::endl;

		std::cout << "[SpimStack] Saved transform to \"" << filename << "\"\n";
	}
	else
		throw std::runtime_error("[SpimStack] Unable to open file \"" + filename + "\"!");

}

void InteractionVolume::loadTransform(const std::string& filename)
{
	std::ifstream file(filename);
	//assert(file.is_open());

	if (!file.is_open())
	{
		std::cerr << "[SpimStack] Unable to load transformation from \"" << filename << "\"!\n";
		return;
	}

	float* m = glm::value_ptr(transform);
	for (int i = 0; i < 16; ++i)
		file >> m[i];

	std::cout << "[SpimStack] Read transform: " << transform << " from \"" << filename << "\"\n";
}

void InteractionVolume::applyTransform(const glm::mat4& t)
{
	transform = t * transform;
}

void InteractionVolume::setRotation(float angle)
{
	transform = glm::translate(glm::mat4(1.f), getBBox().getCentroid());
	transform = glm::rotate(transform, glm::radians(angle), glm::vec3(0, 1, 0));
	transform = translate(transform, getBBox().getCentroid() * -1.f);

}

void InteractionVolume::move(const glm::vec3& delta)
{
	transform = glm::translate(delta) * transform;//  glm::translate(transform, delta);
}

void InteractionVolume::rotate(float angle)
{
	/*
	glm::mat4 T = glm::translate(getBBox().getCentroid());;
	glm::mat4 Ti = glm::translate(-getBBox().getCentroid());;
	*/
	transform = glm::rotate(transform, angle, glm::vec3(0, 1, 0));

}
