#include "SimplePointcloud.h"
#include "Shader.h"

#include <GL/glew.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/io.hpp>

#include <fstream>
#include <algorithm>
#include <iostream>
#include <string>
#include <cstdio>

SimplePointcloud::SimplePointcloud(const std::string& f, const glm::mat4& t) : filename(f)
{
	this->setTransform(t);

	std::string ext = filename.substr(filename.find_last_of("."));

	//std::cout << "[Debug] Filename: \"" << filename << "\", extension: " << ext << std::endl;

	if (ext == ".bin")
		loadBin(filename);
	else if (ext == ".txt")
		loadTxt(filename);
	else
		throw std::runtime_error("Unsupported point cloud \"" + ext + "\"");

	/*
	assert(points.size() == colors.size());
	assert(points.size() == normals.size());
	*/

	pointCount = std::min(vertices.size(), colors.size());
	std::cout << "[File] Read " << pointCount << " points.\n";
	
	bbox.reset(vertices[0]);
	for (size_t i = 1; i < vertices.size(); ++i)
		bbox.extend(vertices[i]);
	
	std::cout << "[Bbox] " << bbox.min << "->" << bbox.max << std::endl;

	glGenBuffers(2, vertexBuffers);
	updateBuffers();
}


SimplePointcloud::~SimplePointcloud()
{
	glDeleteBuffers(2, vertexBuffers);
}

void SimplePointcloud::draw() const
{
	if (!enabled)
		return;

	glPushMatrix();
	glMultMatrixf(glm::value_ptr(getTransform()[0]));

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);

	
	glBindBuffer(GL_ARRAY_BUFFER, vertexBuffers[0]);
	glVertexPointer(3, GL_FLOAT, 0, 0);
	
	glBindBuffer(GL_ARRAY_BUFFER, vertexBuffers[1]);
	glColorPointer(3, GL_FLOAT, 0, 0);
	
	glDrawArrays(GL_POINTS, 0, (GLsizei)pointCount);
	
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);
	

	glPopMatrix();
}

void SimplePointcloud::loadBin(const std::string& filename)
{
	std::ifstream file(filename, std::ios::binary);
	assert(file.is_open());

	uint32_t points = 0;
	file.read(reinterpret_cast<char*>(&points), sizeof(uint32_t));

	std::cout << "[Debug] Reading " << points << " points from \"" << filename << "\" ... ";

	vertices.resize(points);
	colors.resize(points);

	file.read(reinterpret_cast<char*>(glm::value_ptr(vertices[0])), sizeof(glm::vec3)*points);
	file.read(reinterpret_cast<char*>(glm::value_ptr(colors[0])), sizeof(glm::vec3)*points);

	std::cout << "done.\n";

	updateBuffers();
}


void SimplePointcloud::saveBin(const std::string& f)
{
	assert(vertices.size() == colors.size());

	std::cout << "[Pointcloud] Saving pointcloud to \"" << f<< "\" ... ";

	std::ofstream file(f, std::ios::binary);
	uint32_t points = (uint32_t)vertices.size();
	file.write(reinterpret_cast<const char*>(&points), sizeof(uint32_t));
	file.write(reinterpret_cast<const char*>(glm::value_ptr(vertices[0])), sizeof(glm::vec3)*points);
	file.write(reinterpret_cast<const char*>(glm::value_ptr(colors[0])), sizeof(glm::vec3)*points);

	std::cout << "done.\n";

	this->filename = f;
}

void SimplePointcloud::loadTxt(const std::string& filename)
{
	std::ifstream file(filename);
	assert(file.is_open());

	vertices.clear();
	colors.clear();
	
	std::string tmp;
	while (!file.eof())
	{
		glm::vec3 pos, color, normal;

		std::getline(file, tmp);
		/*
		file >> pos.x >> pos.y >> pos.z;
		file >> color.r >> color.g >> color.b;
		file >> normal.x >> normal.y >> normal.z;
		*/
		int r, g, b;

#ifdef _WIN32
		sscanf_s(tmp.c_str(), "%f %f %f %d %d %d %f %f %f", &pos.x, &pos.y, &pos.z, &r, &g, &b, &normal.x, &normal.y, &normal.z);
#else
		sscanf(tmp.c_str(), "%f %f %f %d %d %d %f %f %f", &pos.x, &pos.y, &pos.z, &r, &g, &b, &normal.x, &normal.y, &normal.z);
#endif
		color.r = (float)r;
		color.g = (float)g;
		color.b = (float)b;

		vertices.push_back(pos);
		colors.push_back(color / 255.f);
	}

	updateBuffers();
}

void SimplePointcloud::updateBuffers()
{
	
#ifndef NO_GRAPHICS
	glBindBuffer(GL_ARRAY_BUFFER, vertexBuffers[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3)*vertices.size(), glm::value_ptr(vertices[0]), GL_STATIC_DRAW);
		
	glBindBuffer(GL_ARRAY_BUFFER, vertexBuffers[1]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3)*colors.size(), glm::value_ptr(colors[0]), GL_STATIC_DRAW);
	
	glBindBuffer(GL_ARRAY_BUFFER, 0);

#endif
}



void SimplePointcloud::bakeTransform()
{
	using namespace glm;

	const mat4& T = getTransform();

	for (size_t i = 0; i < vertices.size(); ++i)
		vertices[i] = vec3(T * vec4(vertices[i], 1.f));

	// update bbox
	bbox.reset(vertices[0]);
	for (size_t i = 1; i < vertices.size(); ++i)
		bbox.extend(vertices[i]);

	std::cout << "[Bbox] " << bbox.min << "->" << bbox.max << std::endl;



	updateBuffers();
	setTransform(glm::mat4(1.f));
}