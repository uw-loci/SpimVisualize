#pragma once

#include <string>
#include <glm/glm.hpp>

#include "StackRegistration.h"

struct Config
{

	// the path to the config file
	std::string		configFile;

	// should we save the stack transformations automatically? Will overwrite existing files quietly
	bool			saveStackTransformationsOnExit;

	// the voxel size in nm
	glm::vec3		defaultVoxelSize;
	glm::ivec3		resampleResolution;
	
	// how many steps should the ray tracer take
	unsigned int	raytraceSteps;
	// how big should each step be
	float			raytraceDelta;
	
	// min-max thresholds for contrast settings
	Threshold		threshold;

	inline Config() { setDefaults(); }

	void setDefaults();
	inline void reload() { load(configFile); }
	void load(const std::string& filename);
	void save(const std::string& filename) const;

};
