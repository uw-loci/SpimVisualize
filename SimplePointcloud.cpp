#include "SimplePointcloud.h"
#include "Shader.h"

#include <GL/glew.h>

#include <glm/gtc/type_ptr.hpp>

#include <fstream>
#include <algorithm>
#include <iostream>
#include <string>
#include <cstdio>

SimplePointcloud::SimplePointcloud(const std::string& filename)
{
	// read file here
	PointCloud	points;
	PointCloud	colors;
	PointCloud	normals;

	std::string ext = filename.substr(filename.find_last_of("."));

	//std::cout << "[Debug] Filename: \"" << filename << "\", extension: " << ext << std::endl;

	if (ext == ".bin")
		loadBin(filename, points, normals, colors);
	else if (ext == ".txt")
		loadTxt(filename, points, normals, colors);
	else
		throw std::runtime_error("Unsupported point cloud \"" + ext + "\"");

	/*
	assert(points.size() == colors.size());
	assert(points.size() == normals.size());
	*/

	pointCount = std::min(points.size(), std::min(colors.size(), normals.size()));
	std::cout << "[File] Read " << pointCount << " points.\n";

	bbox.reset(points[0]);
	for (size_t i = 1; i < points.size(); ++i)
		bbox.extend(points[i]);
	

	glGenBuffers(3, vertexBuffers);
	glBindBuffer(GL_ARRAY_BUFFER, vertexBuffers[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3)*points.size(), glm::value_ptr(points[0]), GL_STATIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, vertexBuffers[1]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3)*colors.size(), glm::value_ptr(colors[0]), GL_STATIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, vertexBuffers[2]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3)*normals.size(), glm::value_ptr(normals[0]), GL_STATIC_DRAW);
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

	glPopMatrix();
}

void SimplePointcloud::draw(Shader* sh) const
{
	// assume the shader is already bound ... 

	sh->setMatrix4("transform", transform);
	
	this->draw();

}

void SimplePointcloud::loadBin(const std::string& filename, PointCloud& vertices, PointCloud& normals, PointCloud& colors)
{
	std::ifstream file(filename, std::ios::binary);
	assert(file.is_open());

	uint32_t points = 0;
	file.read(reinterpret_cast<char*>(&points), sizeof(size_t));

	vertices.resize(points);
	normals.resize(points);
	colors.resize(points);

	file.read(reinterpret_cast<char*>(glm::value_ptr(vertices[0])), sizeof(glm::vec3)*points);
	file.read(reinterpret_cast<char*>(glm::value_ptr(normals[0])), sizeof(glm::vec3)*points);
	file.read(reinterpret_cast<char*>(glm::value_ptr(colors[0])), sizeof(glm::vec3)*points);
}


void SimplePointcloud::saveBin(const std::string& filename, const PointCloud& vertices, const PointCloud& normals, const PointCloud& colors)
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

void SimplePointcloud::loadTxt(const std::string& filename, PointCloud& vertices, PointCloud& normals, PointCloud& colors)
{
	std::ifstream file(filename);
	assert(file.is_open());

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

void SimplePointcloud::resaveAsBin(const std::string& filename)
{
	PointCloud	points;
	PointCloud	colors;
	PointCloud	normals;

	std::string ext = filename.substr(filename.find_last_of("."));

	std::cout << "[Debug] Filename: \"" << filename << "\", extension: " << ext << std::endl;

	if (ext == ".bin")
		loadBin(filename, points, normals, colors);
	else if (ext == ".txt")
		loadTxt(filename, points, normals, colors);
	else
		throw std::runtime_error("Unsupported point cloud \"" + ext + "\"");
	
	std::string newName = filename.substr(0, filename.find_last_of(".")) + ".bin";

	std::cout << "[Debug] Resaving \"" << filename << "\" as \"" << newName << "\".\n"; 
	saveBin(newName, points, normals, colors);

}