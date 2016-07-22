#include "Config.h"

#include <iostream>
#include <fstream>

#include <glm/gtx/io.hpp>

using namespace std;

void Config::setDefaults()
{
	configFile = "./config.cfg";

	defaultVoxelSize = glm::vec3(1, 1, 1);
	raytraceSteps = 2000;
	raytraceDelta = 1;

	saveStackTransformationsOnExit = true;
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
		{
			getline(file, temp);
			continue;
		}

		if (temp.empty())
			continue;


		if (temp == "saveTransforms")
		{
			file >> saveStackTransformationsOnExit;
			cout << "[Config] Save stack transforms on exit: " << std::boolalpha << saveStackTransformationsOnExit << endl;
		}

		if (temp == "voxelSize")
		{
			file >> defaultVoxelSize.x >> defaultVoxelSize.y >> defaultVoxelSize.z;
			cout << "[Config] Default voxel size: " << defaultVoxelSize << endl;
		}
		if (temp == "raytraceSteps")
		{
			file >> raytraceSteps;
			cout << "[Config] Raytrace steps: " << raytraceSteps << endl;
		}

		if (temp == "raytraceDelta")
		{
			file >> raytraceDelta;
			cout << "[Config] Ray trace delta: " << raytraceDelta << endl;
		}
	
	}

}


void Config::save(const string& filename) const
{
	ofstream file(filename);
	if (!file.is_open())
		throw runtime_error("Unable to open file \"" + filename + "\"!");

	file << "# general\n";
	file << "saveTransforms " << (int)saveStackTransformationsOnExit << endl;

	file << "# ray trace settings\n";
	file << "voxelSize " << defaultVoxelSize.x << " " << defaultVoxelSize.y << " " << defaultVoxelSize.z << endl;
	file << "raytraceSteps " << raytraceSteps << endl;
	file << "raytraceDelta " << raytraceDelta << endl;
		
}