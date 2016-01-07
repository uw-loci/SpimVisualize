#include "StackTransformationSolver.h"

#include <cassert>
#include <algorithm>
#include <random>
#include <iostream>

#include <GL/glew.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform2.hpp>

#include "Framebuffer.h"

IStackTransformationSolver::IStackTransformationSolver() : currentSolution(-1)
{
}

bool IStackTransformationSolver::nextSolution()
{
	if (solutions.empty())
		return false;

	++currentSolution;
	if (currentSolution == solutions.size())
	{
		// reset to last valid solution
		--currentSolution;
		return false;
	}

	return true;
}

void IStackTransformationSolver::resetSolutions()
{
	solutions.clear();
	currentSolution = -1;
}

bool IStackTransformationSolver::hasValidCurrentSolution() const
{
	return !solutions.empty() && (currentSolution >= 0) && (currentSolution < solutions.size());
}

void IStackTransformationSolver::recordCurrentScore(double s)
{
	if (hasValidCurrentSolution())
	{
		std::cout << "[Solver] Recording score of " << s << " for current solution.\n";
		solutions[currentSolution].score += s;
	}
}

void IStackTransformationSolver::recordCurrentScore(Framebuffer* fbo)
{
	fbo->bind();
	glReadBuffer(GL_COLOR_ATTACHMENT0);

	std::vector<glm::vec4> pixels(fbo->getWidth()*fbo->getHeight());
	glReadPixels(0, 0, fbo->getWidth(), fbo->getHeight(), GL_RGBA, GL_FLOAT, glm::value_ptr(pixels[0]));
	fbo->disable();

	double value = 0;

	for (size_t i = 0; i < pixels.size(); ++i)
	{
		glm::vec3 color(pixels[i]);
		value += glm::dot(color, color);
	}

	std::cout << "[Solver] Read back score: " << value << std::endl;
	glReadBuffer(GL_BACK);

	recordCurrentScore(value);
}


const IStackTransformationSolver::Solution& IStackTransformationSolver::getCurrentSolution() const
{
	if (!hasValidCurrentSolution())
		throw std::runtime_error("Solver has no valid current solution");
	
	return solutions[currentSolution];
}

const IStackTransformationSolver::Solution& IStackTransformationSolver::getBestSolution()
{
	assert(!solutions.empty());

	std::sort(solutions.begin(), solutions.end());
	return solutions.back();
}

void UniformSamplingSolver::createCandidateSolutions()
{
	using namespace glm;

	resetSolutions();

	std::mt19937 rng;
	
	// single axis alignment: dx,dy,dz, ry
	int transform = rng() % 4;

	
	// right now just check dx+dz
	if (transform == 1)
		transform = 0;
	if (transform == 3)
		transform = 2;

	std::cout << "[Uniform solver] Creating transformations for: " << std::endl;

	if (transform == 3)
	{
		/*
		//vec3 d = stacks[currentStack]->getBBox().getCentroid();
		vec3 d = interactionVolumes[currentVolume]->getBBox().getCentroid();
		mat4 T = glm::translate(vec3(d.x, 0.f, d.z));
		*/

		// rotation
		for (int a = -100; a <= 100; ++a)
		{
			Solution s;
			s.score = 0;
			s.id = a;
			s.matrix = glm::rotate(radians((float)a / 10.f), vec3(0, 1, 0));

			solutions.push_back(s);
		}
	}
	else
	{
		for (int x = -100; x <= 100; ++x)
		{
			float dx = (float)x / 5.f;
			vec3 v(0.f);
			v[transform] = dx;

			Solution s;
			s.score = 0;
			s.id = x;
			s.matrix = translate(v);
			
			solutions.push_back(s);
		}
	}
	
	std::cout << "[Uniform solver] Created " << solutions.size() << " new candidate transforms.\n";
	currentSolution = 0;
}