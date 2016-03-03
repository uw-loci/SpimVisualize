#pragma once

#include <glm/glm.hpp>
#include <vector>

class SpimStack;


class IStackSampler
{
public:
	IStackSampler(const SpimStack* input, SpimStack* output);
	virtual ~IStackSampler();

	virtual void draw() = 0;
	virtual void process() = 0;

	virtual bool isDone() const = 0;

protected:
	const SpimStack*	input;
	SpimStack*			output;
};

class CPUStackSampler : public IStackSampler
{
public:
	CPUStackSampler(const SpimStack* input, SpimStack* output);
	
	virtual void draw();
	virtual void process();

	inline bool isDone() const { return stage == SAMPLER_STAGE_FIN; }




private:
	size_t				sampleCount;

	enum ProcessStage
	{
		SAMPLER_STAGE_OUTPUT_TO_WORLD,
		SAMPLER_STAGE_WORLD_TO_INPUT,
		SAMPLER_STAGE_INPUT_TO_VALUE,
		SAMPLER_STAGE_FIN
	}					stage;


	
	std::vector<glm::vec4>		worldCoords;
	std::vector<glm::ivec3>		inputCoords;
	std::vector<double>			inputValues;


	void createWorldCoords();
	void createInputIndex();
	void readValues();

};


class GPUStackSampler : public IStackSampler
{
public:
	GPUStackSampler(const SpimStack* input, SpimStack* output);

	virtual void draw();
	virtual void process();

	virtual bool isDone() const;

private:
	std::vector<glm::vec4>		samples;
};