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

	std::ifstream file(filename);
	assert(file.is_open());
	

	bbox.reset();

	// read file here
	std::vector<glm::vec3>		points;
	std::vector<glm::vec3>		colors;
	std::vector<glm::vec3>		normals;

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

		points.push_back(pos);
		colors.push_back(color / 255.f);
		normals.push_back(glm::normalize(normal));

		bbox.extend(pos);

	}
	
	pointCount = std::min(points.size(), std::min(colors.size(), normals.size()));
	std::cout << "[File] Read " << pointCount << " points.\n";


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

