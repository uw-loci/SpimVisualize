
#include <iostream>
#include <random>

#include "AABB.h"
#include "FreeImage.h"
#include "SpimStack.h"

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/transform2.hpp>

using namespace glm;


int main(int argc, const char** argv)
{

	if (argc == 1)
	{
		std::cerr << "No input file given.\n";
		std::cerr << "Usage: " << argv[0] << " <filename>\n";

#ifdef _WIN32
		system("pause");
#endif

		return 1;
	}

	std::mt19937 rng;


	// load spim stack
	SpimStack* reference = new SpimStackU16;
	reference->load(argv[1]);


	// create n slices through resampling
	const size_t SLICES = 5;

	// basic resolution
	ivec3 res(1000, 1000, 150);

	for (size_t i = 0; i < SLICES; ++i)
	{
		// create random transformation here

		// each transform should be rotated
		float angle = mix(-70.f, 70.f, (float)i / SLICES);
		// add a random offset to each angle

		float da = (float)rng() / rng.max();
		da *= 2.f;
		da -= 1.f;
		da *= 5.f;
		angle += da;

		// rotate only around the y axis
		mat4 R = rotate(radians(angle), vec3(0, 1, 0));
		std::cout << "[Transform " << i << "/" << SLICES << "] Angle: " << angle << std::endl;


		// create translation
		vec3 d(0.f);	
		d.x = (float)rng() / rng.max();
		d.y = (float)rng() / rng.max();
		d.z = (float)rng() / rng.max();
	
		d *= 2.f;
		d -= vec3(1.f);
		d *= vec3(20, 5, 20);
		
		mat4 T = translate(d);
		std::cout << "[Transform " << i << "/" << SLICES << "] DX " << d.x << " DY " << d.y << " DZ " << d.z << std::endl;


		SpimStack* newStack = new SpimStackU16;
		newStack->setTransform(T * R);
		newStack->setContent(res, 0);
		

		std::cout << "[Resampling] ";

		unsigned int dots = res.x*res.y*res.z / 20;
		unsigned int counter = 0;

		// resample original stack here
		for (int x = 0; x < res.x; ++x)
		{
			for (int y = 0; y < res.y; ++y)
			{
				for (int z = 0; z < res.z; ++z)
				{
					// transform point to world space
					vec3 pt = newStack->getWorldPosition(ivec3(x, y, z));

					double value = 0;
					if (reference->isInsideVolume(pt))
					{
						// sample original volume
						value = reference->getSample(pt);
					}


					newStack->setSample(ivec3(x, y, z), value);
				
					if (counter >= dots)
					{
						std::cout << ".";
						counter = 0;
					}
					else
						++counter;
				
				}

			}


		}

		std::cout << " done.\n";


		// create filename
		std::string filename = std::string("./phantom_") + std::to_string(i) + std::string(".tiff");
		newStack->saveTransform(filename + ".registration");
		newStack->save(filename);
		

		// save stack

		// and transformation



		delete newStack;
	}


	delete reference;

#ifdef _WIN32
	system("pause");
#endif

	return 0;
}