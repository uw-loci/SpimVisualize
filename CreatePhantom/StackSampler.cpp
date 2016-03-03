#include "StackSampler.h"
#include <SpimStack.h>

#include <iostream>

#include <glm/gtc/type_ptr.hpp>
#include <GL/glew.h>

IStackSampler::IStackSampler(const SpimStack* i, SpimStack* o) : input(i), output(o)
{
}

IStackSampler::~IStackSampler()
{
}

CPUStackSampler::CPUStackSampler(const SpimStack* i, SpimStack* o) : IStackSampler(i, o), stage(SAMPLER_STAGE_OUTPUT_TO_WORLD)
{

	sampleCount = output->getWidth()*output->getHeight()*output->getDepth();
}

void CPUStackSampler::process()
{
	createWorldCoords();
	createInputIndex();
	readValues();
}

void CPUStackSampler::createWorldCoords()
{
	std::cout << "[Sampler] Creating world coords ... ";
	worldCoords.resize(sampleCount);

	for (size_t i = 0; i < sampleCount; ++i)
		worldCoords[i] = glm::vec4(output->getWorldPosition(i), 1.f);

	stage = SAMPLER_STAGE_WORLD_TO_INPUT;

	std::cout << "done.\n";
}

void CPUStackSampler::createInputIndex()
{
	std::cout << "[Sampler] Creating input index ... ";
	inputCoords.resize(sampleCount, glm::ivec3(-1));
	
	for (size_t i = 0; i < sampleCount; ++i)
	{
		if (input->isInsideVolume(worldCoords[i]))
			inputCoords[i] = input->getStackVoxelCoords(worldCoords[i]);


	}

	stage = SAMPLER_STAGE_INPUT_TO_VALUE;

	std::cout << "done.\n";
}

void CPUStackSampler::readValues()
{
	std::cout << "[Sampler] Read input values ... ";
	inputValues.resize(sampleCount, 0);

	for (size_t i = 0; i < sampleCount; ++i)
	{
		if (inputCoords[i].x >= 0)
			inputValues[i] = input->getSample(inputCoords[i]);

	}

	stage = SAMPLER_STAGE_FIN;

	std::cout << "done.\n";
}

void CPUStackSampler::draw()
{
	glEnableClientState(GL_VERTEX_ARRAY);

	glColor3f(1, 1, 1);
	glVertexPointer(4, GL_FLOAT, 0, glm::value_ptr(worldCoords[0]));
	glDrawArrays(GL_POINTS, 0, sampleCount);

	glDisableClientState(GL_VERTEX_ARRAY);
}