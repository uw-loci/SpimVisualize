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

SimplePointcloud::SimplePointcloud(const std::string& filename, const glm::mat4& t)
{
	this->transform = t;

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

	pointCount = std::min(vertices.size(), std::min(colors.size(), normals.size()));
	std::cout << "[File] Read " << pointCount << " points.\n";
	
	bbox.reset(vertices[0]);
	for (size_t i = 1; i < vertices.size(); ++i)
		bbox.extend(vertices[i]);
	
	std::cout << "[Bbox] " << bbox.min << "->" << bbox.max << std::endl;

	glGenBuffers(3, vertexBuffers);
	glBindBuffer(GL_ARRAY_BUFFER, vertexBuffers[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3)*vertices.size(), glm::value_ptr(vertices[0]), GL_STATIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, vertexBuffers[1]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3)*colors.size(), glm::value_ptr(colors[0]), GL_STATIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, vertexBuffers[2]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3)*normals.size(), glm::value_ptr(normals[0]), GL_STATIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
}


SimplePointcloud::~SimplePointcloud()
{
	glDeleteBuffers(3, vertexBuffers);
}

void SimplePointcloud::draw() const
{
	glPushMatrix();
	glMultMatrixf(glm::value_ptr(transform[0]));
	
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);

	glBindBuffer(GL_ARRAY_BUFFER, vertexBuffers[0]);
	glVertexPointer(3, GL_FLOAT, 0, 0);
	
	glBindBuffer(GL_ARRAY_BUFFER, vertexBuffers[1]);
	glColorPointer(3, GL_FLOAT, 0, 0);

	glBindBuffer(GL_ARRAY_BUFFER, vertexBuffers[2]);
	glNormalPointer(GL_FLOAT, 0, 0);

	glDrawArrays(GL_POINTS, 0, pointCount);
	
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);

	glPopMatrix();
}

void SimplePointcloud::loadBin(const std::string& filename)
{
	std::ifstream file(filename, std::ios::binary);
	assert(file.is_open());

	uint32_t points = 0;
	file.read(reinterpret_cast<char*>(&points), sizeof(size_t));

	std::cout << "[Debug] Reading " << points << " points from \"" << filename << "\".\n";

	vertices.resize(points);
	normals.resize(points);
	colors.resize(points);

	file.read(reinterpret_cast<char*>(glm::value_ptr(vertices[0])), sizeof(glm::vec3)*points);
	file.read(reinterpret_cast<char*>(glm::value_ptr(normals[0])), sizeof(glm::vec3)*points);
	file.read(reinterpret_cast<char*>(glm::value_ptr(colors[0])), sizeof(glm::vec3)*points);
}


void SimplePointcloud::saveBin(const std::string& filename)
{
	assert(vertices.size() == normals.size());
	assert(vertices.size() == colors.size());

	std::ofstream file(filename, std::ios::binary);
	uint32_t points = vertices.size();
	file.write(reinterpret_cast<const char*>(&points), sizeof(size_t));
	file.write(reinterpret_cast<const char*>(glm::value_ptr(vertices[0])), sizeof(glm::vec3)*points);
	file.write(reinterpret_cast<const char*>(glm::value_ptr(normals[0])), sizeof(glm::vec3)*points);
	file.write(reinterpret_cast<const char*>(glm::value_ptr(colors[0])), sizeof(glm::vec3)*points);
}

void SimplePointcloud::loadTxt(const std::string& filename)
{
	std::ifstream file(filename);
	assert(file.is_open());

	vertices.clear();
	colors.clear();
	normals.clear();

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
		sscanf_s(tmp.c_str(), "%f %f %f %d %d %d %f %f %f", &pos.x, &pos.y, &pos.z, &r, &g, &b, &normal.x, &normal.y, &normal.z);
		color.r = r;
		color.g = g;
		color.b = b;

		vertices.push_back(pos);
		colors.push_back(color / 255.f);
		normals.push_back(glm::normalize(normal));
	}

}
