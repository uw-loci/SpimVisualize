#include "Config.h"

#include <iostream>
#include <fstream>

using namespace std;

void Config::setDefaults()
{
	configFile = "./config.cfg";
	shaderPath = "./shaders/";

	defaultVoxelSize = glm::vec3(1, 1, 1);
	raytraceSteps = 2000;
	raytraceDelta = 1;

	resampleResolution = glm::ivec3(512, 512, 64);

	threshold.set(0, 255);
}


void Config::load(const string& filename)
{
	ifstream file(filename);
	if (!file.is_open())
		throw runtime_error("Unable to open file \"" + filename + "\"!");


	cout << "[Config] Loading config file \"" << filename << "\" ... \n";

	configFile = filename;

	while (!file.eof())
	{
		string temp;
		file >> temp;
		if (temp[0] == '#')
			continue;

		if (temp == "shaderPath")
			file >> shaderPath;
		if (temp == "voxelSize")
			file >> defaultVoxelSize.x >> defaultVoxelSize.y >> defaultVoxelSize.z; 
		if (temp == "raytraceSteps")
			file >> raytraceSteps;
		if (temp == "raytraceDelta")
			file >> raytraceDelta;

		if (temp == "minThreshold")
			file >> threshold.min;
		if (temp == "maxThreshold")
			file >> threshold.max;

		if (temp == "resampleResolution")
			file >> resampleResolution.x >> resampleResolution.y >> resampleResolution.z;

	}


	threshold.set(threshold.min, threshold.max);
}


void Config::save(const string& filename) const
{
	ofstream file(filename);
	if (!file.is_open())
		throw runtime_error("Unable to open file \"" + filename + "\"!");


	file << "# general settings\n";
	file << "shaderPath " << shaderPath << endl;

	file << "# ray trace settings\n";
	file << "voxelSize " << defaultVoxelSize.x << " " << defaultVoxelSize.y << " " << defaultVoxelSize.z << endl;
	file << "raytraceSteps " << raytraceSteps << endl;
	file << "raytraceDelta " << raytraceDelta << endl;


	file << "# contrast settings\n";
	file << "minThreshold " << threshold.min << endl;
	file << "maxThreshold " << threshold.max << endl;


	file << "# resampling\n";
	file << "resampleResolution " << resampleResolution.x << " " << resampleResolution.y << " " << resampleResolution.z << endl;
}