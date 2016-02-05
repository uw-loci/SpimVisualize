
#include <iostream>

#include "AABB.h"
#include "FreeImage.h"
#include "SpimStack.h"

#include <glm/glm.hpp>

int main(int argc, const char** argv)
{

	if (argc == 1)
	{
		std::cerr << "No input file given.\n";
		std::cerr << "Usage: " << argv[0] << " <filename>\n";
		return 1;
	}



	// load spim stack
	SpimStack* reference = new SpimStackU16;
	reference->load(argv[1]);


	// create n slices through resampling
	const size_t SLICES = 5;

	// basic resolution
	glm::ivec3 res(1000, 1000, 150);

	for (size_t i = 0; i < SLICES; ++i)
	{
		// create random transformation
		glm::mat4 transform(1.f);



		SpimStack* newStack = new SpimStackU16;
		newStack->transform = transform;
		newStack->setContent(res, 0);


		// resample original stack here
		

		// save stack

		// and transformation



		delete newStack;
	}


	delete reference;

	return 0;
}