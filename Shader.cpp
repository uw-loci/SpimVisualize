/*
	GLSL Shader abstraction

	MIT License

	Copyright (c) 2009, Markus Broecker <mbrckr@gmail.com>

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in
	all copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
	THE SOFTWARE.
 */

#include "Shader.h"
#include <GL/glew.h>
#include <GL/gl.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <cassert>
#include <iomanip>
#include <algorithm>

#include <glm/gtc/type_ptr.hpp>

// long shaders are looooooooooooooooooooong
const unsigned int BUFFER_SIZE = 2048;


// maybe this is only needed on win32 as the include does not support all features of c++11... 

/*
namespace std
{
	static inline std::string to_string(unsigned int v)
	{
#ifdef LINUX_BUILD
		return std::to_string((unsigned long long)v);
#else
		return std::to_string((_ULonglong)v);
#endif
	}
}
*/


namespace ShaderLog
{

static void info(const std::string& msg)
{
	std::clog << "[Shader Info]: " << msg << std::endl;
}

static void compileInfo(const std::string& msg)
{
	std::clog << "[Shader Compilation]: " << msg << std::endl; 
}

static void error(const std::string& msg)
{
	std::cerr << "[Shader Error]: " << msg << std::endl;
}

std::vector<std::string>& split(const std::string &s, char delim, std::vector<std::string> &elems) 
{
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim))
	{
		item.erase(item.find_last_not_of(" \n\r\t")+1);
		if (!item.empty()) 
			elems.push_back(item);
	}
	return elems;
}


}


static void compileShader(const std::string& fileName, GLint shader, const std::vector<std::string>& defines)
{
	std::vector<std::string>	contents;

	std::ifstream file;
	file.open( fileName.c_str() );

	if (!file.is_open())
	{
		ShaderLog::error("Error opening file " + fileName + ".");
	}

	std::string line;
	while (!file.eof())
	{
		std::getline( file, line );
		line.append("\n");
		contents.push_back( line );
	}

	file.close();

	
	// preprocessing
	if (!defines.empty())
	{

		// find the #version token
		auto it = contents.begin();
		while (it->find("#version") == std::string::npos || it == contents.end())
			++it;

		// not found -- insert at the beginning
		if (it == contents.end())
		{
			contents.insert(contents.end(), defines.begin(), defines.end());
		}
		
		// found -- insert after
		contents.insert(++it, defines.begin(), defines.end());
		it += defines.size();
		


		// parse the rest of the lines
		
		// first create a map of all defines and tokens
		std::vector<std::string> definedTokens;
		for (auto d = defines.begin(); d != defines.end(); ++d)
		{
			std::string token = d->substr(d->find("#define ") + 8);
			token = token.substr(0, token.find_first_of(" "));

			//std::cout << "[Debug] Token: [" << token << "]\n";
			definedTokens.push_back(token);

		}


		while (it != contents.end())
		{
			size_t loc = it->find("#define");
			if (loc != std::string::npos)
			{
				// extract token
				std::string token = it->substr(loc + 8);
				token = token.substr(0, token.find_first_of(" "));

				//std::cout << "[Debug] Found token: [" << token << "]\n";

				if (std::find(definedTokens.begin(), definedTokens.end(), token) != definedTokens.end())
				{
					//std::cout << "[Preprocess] Erasing line " << std::distance(contents.begin(), it) << ": \"" << *it << "\"\n";
					
					it = contents.erase(it);
				}
			}
			else
				++it;

			
		}






		
	}


	/*
	std::cerr << "[Debug] Shader source:\n";
	for (size_t i = 0; i < contents.size(); ++i)
	{
		std::cerr << "[Sauce] " << std::setw(2) << i << ": " << contents[i]; // << std::endl;
	}
	*/


	const char *glLines[BUFFER_SIZE];
	for (unsigned int i = 0; i < contents.size(); ++i)
		glLines[i] = contents[i].c_str();

	glShaderSource( shader, (GLsizei)contents.size(), glLines , 0 );
	glCompileShader( shader );

	char buffer[BUFFER_SIZE];
	memset( buffer, 0, BUFFER_SIZE );
	int length = 0;

	glGetShaderInfoLog( shader, BUFFER_SIZE, &length, buffer );
	if (length > 0)
	{
		std::vector <std::string> log;
		ShaderLog::split(std::string(buffer), '\n', log);

		if (log.size() > 1)
			ShaderLog::compileInfo("File: " + fileName);

		for (int i = 0; i < log.size(); ++i)
			ShaderLog::compileInfo(log[i]);

	}
}

Shader::Shader(const std::string& vpFile, const std::string& fpFile) : mGeometryShader(0), linkedSuccessfully(false), mVertexSource(vpFile), mFragmentSource(fpFile)
{
	mProgram = glCreateProgram();

	mVertexShader = glCreateShader(GL_VERTEX_SHADER);
	mFragmentShader = glCreateShader(GL_FRAGMENT_SHADER);


	reload();
}

Shader::Shader(const std::string& vpFile, const std::string& gpFile, const std::string& fpFile) : linkedSuccessfully(false), mVertexSource(vpFile), mGeometrySource(gpFile), mFragmentSource(fpFile)
{
	mProgram = glCreateProgram();

	mVertexShader = glCreateShader(GL_VERTEX_SHADER);
	mGeometryShader = glCreateShader(GL_GEOMETRY_SHADER);
	mFragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

	reload();
}

Shader::Shader(const std::string& vpFile, const std::string& fpFile, const std::vector<std::string>& defines) : mGeometryShader(0), linkedSuccessfully(false), mVertexSource(vpFile), mFragmentSource(fpFile), mDefines(defines)
{
	mProgram = glCreateProgram();

	mVertexShader = glCreateShader(GL_VERTEX_SHADER);
	mFragmentShader = glCreateShader(GL_FRAGMENT_SHADER);


	reload();
}

Shader::Shader(const std::string& vpFile, const std::string& gpFile, const std::string& fpFile, const std::vector<std::string>& defines) : linkedSuccessfully(false), mVertexSource(vpFile), mGeometrySource(gpFile), mFragmentSource(fpFile), mDefines(defines)
{
	mProgram = glCreateProgram();

	mVertexShader = glCreateShader(GL_VERTEX_SHADER);
	mGeometryShader = glCreateShader(GL_GEOMETRY_SHADER);
	mFragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

	reload();
}

Shader::~Shader()
{
	glDeleteShader( mVertexShader );
	if (glIsShader(mGeometryShader))
		glDeleteShader( mGeometryShader );
	glDeleteShader( mFragmentShader );
	glDeleteProgram( mProgram );
}

void Shader::reload()
{
	compileShader(mVertexSource, mVertexShader, mDefines);
	if (glIsShader(mGeometryShader))
		compileShader(mGeometrySource, mGeometryShader, mDefines);
	compileShader(mFragmentSource, mFragmentShader, mDefines);


	link();

}

void Shader::link()
{
	// attach all shaders we have
	glAttachShader( mProgram, mVertexShader );
	if (glIsShader(mGeometryShader))
		glAttachShader( mProgram, mGeometryShader );
	glAttachShader( mProgram, mFragmentShader );

	glLinkProgram( mProgram );


	// check for errors
	char buffer[BUFFER_SIZE];
	memset( buffer, 0, BUFFER_SIZE );
	int length = 0;

	glGetProgramInfoLog( mProgram, BUFFER_SIZE, &length, buffer );
	if (length > 0)
		ShaderLog::info( std::string(buffer) );

	// validate
	glValidateProgram( mProgram );
	int status;
	glGetProgramiv( mProgram, GL_VALIDATE_STATUS, &status );
	if (status == GL_FALSE)
		ShaderLog::error("Shader validation failed.\n");
#ifndef LINUX_BUILD
	else
	{
		ShaderLog::info("Created shader " + std::to_string(mProgram));
		linkedSuccessfully = true;
	}
#endif

	mUniformLocations.clear();
}


void Shader::bind() const
{
	glUseProgram( mProgram );
}

void Shader::disable() const
{
	glUseProgram( 0 );
}

int Shader::getUniform(const std::string& name) const
{
	IntMap::iterator loc = mUniformLocations.find(name);
	if (loc == mUniformLocations.end())
	{
		int u = glGetUniformLocation( mProgram, name.c_str());
#ifndef LINUX_BUILD
		if (u < 0)
			ShaderLog::error("Uniform location \"" + name + "\" not found in shader " + std::to_string(mProgram));
#endif

		// this assumes that the insertion will succeeed
		loc = mUniformLocations.insert( std::make_pair(name, u) ).first;
	}
	return loc->second;
}

void Shader::setUniform(const std::string& name, int i) const
{
	glUniform1i( getUniform(name), i );
}

void Shader::setUniform(const std::string& name, float f) const
{
	glUniform1f( getUniform(name), f );
}

void Shader::setUniform(const std::string& name, float x, float y) const
{
	glUniform2f( getUniform(name), x, y );
}

void Shader::setUniform(const std::string& name, float x, float y, float z) const
{
	glUniform3f( getUniform(name), x, y, z );
}

void Shader::setUniform(const std::string& name, float x, float y, float z, float w) const
{
	glUniform4f( getUniform(name), x, y, z, w );
}


void Shader::setUniform(const std::string& name, const glm::vec2& v) const
{
	glUniform2f( getUniform( name ), v.x, v.y );
}

void Shader::setUniform(const std::string& name, const glm::vec3& v) const
{
	glUniform3f( getUniform( name ), v.x, v.y, v.z );
}

void Shader::setUniform(const std::string& name, const glm::vec4& v) const
{
	glUniform4f( getUniform( name ), v.x, v.y, v.z, v.w );
}


void Shader::setMatrix4(const std::string& name, const glm::mat4& m) const
{
	glUniformMatrix4fv( getUniform(name), 1, GL_FALSE, glm::value_ptr(m) );
}

void Shader::setMatrix4(const std::string& name, const glm::dmat4& m) const
{
	glUniformMatrix4dv(getUniform(name), 1, GL_FALSE, glm::value_ptr(m));
}


void Shader::setMatrix4(const std::string& name, const float* matrix) const
{
	glUniformMatrix4fv( getUniform(name), 1, GL_FALSE, matrix);
}

void Shader::setMatrices4(const std::string& name, const std::vector<glm::mat4>& m) const
{
	glUniformMatrix4fv(getUniform(name), (GLsizei)m.size(), GL_FALSE, glm::value_ptr(m[0]));
}


void Shader::setTexture1D(const std::string& name, unsigned int textureId, unsigned int textureUnit) const
{
	glActiveTexture(GL_TEXTURE0 + textureUnit);
	glBindTexture(GL_TEXTURE_1D, textureId);
	glUniform1i( getUniform(name), textureUnit );
}

void Shader::setTexture2D(const std::string& name, unsigned int textureId, unsigned int textureUnit) const
{
	glActiveTexture(GL_TEXTURE0 + textureUnit);
	glBindTexture(GL_TEXTURE_2D, textureId);
	glUniform1i( getUniform(name), textureUnit );
}

void Shader::setTextureCube(const std::string& name, unsigned int texId, unsigned int unit) const
{
	glActiveTexture(GL_TEXTURE0 + unit);
	glBindTexture(GL_TEXTURE_CUBE_MAP, texId);
	glUniform1i( getUniform(name), unit );
}

void Shader::setAttributeLocation(unsigned int attributeLocation, const std::string& name)
{
	glBindAttribLocation(mProgram, attributeLocation, name.c_str());
}

int Shader::getAttributeLocation(const std::string& name)
{
	auto loc = mAttributeLocations.find(name);
	if (loc == mAttributeLocations.end())
	{
		int u = glGetAttribLocation(mProgram, name.c_str());
#ifndef LINUX_BUILD
		if (u < 0)
			ShaderLog::error("Attribute location \"" + name + "\" not found in shader " + std::to_string(mProgram));
#endif

		// this assumes that the insertion will succeeed
		loc = mAttributeLocations.insert(std::make_pair(name, u)).first;
	}
	return loc->second;
}

void Shader::setUniform(const std::string& name, const glm::ivec2& v) const
{
	glUniform2i(getUniform(name), v.x, v.y);
}

void Shader::setUniform(const std::string& name, int i, int j) const
{
	glUniform2i(getUniform(name), i, j);

}
