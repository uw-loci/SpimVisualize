#pragma once

#include <string>
#include <glm/glm.hpp>

#include "StackRegistration.h"

struct Config
{
	std::string		configFile;

	glm::vec3		defaultVoxelSize;
	glm::ivec3		resampleResolution;
	
	std::string		shaderPath;

	unsigned int	raytraceSteps;
	float			raytraceDelta;
	
	Threshold		threshold;

	inline Config() { setDefaults(); }

	void setDefaults();
	inline void reload() { load(configFile); }
	void load(const std::string& filename);
	void save(const std::string& filename) const;

};
