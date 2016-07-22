#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>

struct Config
{

	// the path to the config file
	std::string		configFile;

	// should we save the stack transformations automatically? Will overwrite existing files quietly
	bool			saveStackTransformationsOnExit;

	// the voxel size in nm
	glm::vec3		defaultVoxelSize;
	
	// how many steps should the ray tracer take
	unsigned int	raytraceSteps;
	// how big should each step be
	float			raytraceDelta;
	
	inline Config() { setDefaults(); }

	void setDefaults();
	inline void reload() { load(configFile); }
	void load(const std::string& filename);
	void save(const std::string& filename) const;

};
