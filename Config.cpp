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

	resampleResolution = glm::ivec3(512, 512, 64);

	threshold.set(0, 255);

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
		if (temp == "minThreshold")
		{
			file >> threshold.min;
			cout << "[Config] Threshold.min: " << threshold.min << endl;
		}
		if (temp == "maxThreshold")
		{
			file >> threshold.max;
			cout << "[Config] Threshold.max: " << threshold.max << endl;
		}
		if (temp == "resampleResolution")
		{
			file >> resampleResolution.x >> resampleResolution.y >> resampleResolution.z;
			cout << "[Config] Resample resolution: " << resampleResolution << endl;
		}

		if (temp == "stack")
		{
			InitialStackPosition isp;

			file >> isp.filename;
			file >> isp.x;
			file >> isp.y;
			file >> isp.z;
			file >> isp.rotY;
		
			stackPositions.push_back(isp);
		
			cout << "[Config] Adding default location for stack \"" << isp.filename << "\": " << isp.x << "," << isp.y << "," << isp.z << "," << isp.rotY << "\n";
		}

	}


	threshold.set(threshold.min, threshold.max);
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


	file << "# contrast settings\n";
	file << "minThreshold " << threshold.min << endl;
	file << "maxThreshold " << threshold.max << endl;


	file << "# resampling\n";
	file << "resampleResolution " << resampleResolution.x << " " << resampleResolution.y << " " << resampleResolution.z << endl;


	file << "# initial stack locations\n";
	file << "# format: stack <SPIM file> <Offset X> <Offset Y> <Offset Z> <Rotation Y>\n";
	file << "# offsets are in mm, rotation in degrees\n";
	file << "# example: spim_TL01_Angle.1.ome.tiff 0 -5 0 20\n";
	for (size_t i = 0; i < stackPositions.size(); ++i)
	{
		const InitialStackPosition& p = stackPositions[i];
		file << "stack " << p.filename << " " << p.x << " " << p.y << " " << p.z << " " << p.rotY << endl;
	}

}