/*
	OpenGL Frambuffer Object (FBO) abstraction. Includes fp16 framebuffers

	MIT License

	Copyright (c) 2009-2014, Markus Broecker <mbrckr@gmail.com>

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

#ifndef FRAMEBUFFER_INCLUDED
#define FRAMEBUFFER_INCLUDED

#include <stdexcept>
#include <string>

#include <GL/glew.h>
#include <GL/gl.h>

class Framebuffer
{
public:
		
	/// \name Construction
	/// \{

	/// Constructor
	/**	Creates a new framebuffer object that can be bound. It textures will
		be used as render target for OpenGL writes and they can be bound as
		textures.
		@param width Width of all the textures
		@param height Height of all textures
		@param channels The amount of channels _per texture_ (GL_RGB, GL_RGBA, etc)
		@param type The datatype for all textures (GL_FLOAT, GL_UNSIGNED_BYTE)
		@param colorbufferCount Number of color buffers/textures to create (GL_LINEAR, GL_MIPMAP)
		@param filter the mag/min filter to use
		@param depthTexture Is the depth buffer readable or not (default: not readable)
	*/	
	Framebuffer(unsigned int width, unsigned int height, GLenum internatlFormat = GL_RGBA, GLenum type = GL_UNSIGNED_BYTE, unsigned int colorbufferCount = 1, GLenum filter=GL_NEAREST_MIPMAP_LINEAR, bool depthTexture=false);
	/// D'Tor
	~Framebuffer();
	
	/// \}
	
	/// \name Write control
	/// \{
		
	/// Binds this framebuffer and sets up OpenGL to write to it
	void bind();
	/// Disables this framebuffer and returns to backbuffer rendering
	void disable();
	
	/// }
	
	/// \name Data access
	/// \{
	
	unsigned int getColorbufferCount() const;
	unsigned int getColorbuffer(unsigned int index = 0) const;
	unsigned int getDepthbuffer() const;

	unsigned int getWidth() const;
	unsigned int getHeight() const;
	
	GLenum getMinFilterType() const;
		
	/// \}
	
private:
	unsigned int 	mWidth, mHeight;
	unsigned int	mBuffer;
	
	GLenum			mInternalFormat;
	GLenum			mType;
	GLenum			mFilter;
	
	unsigned int* 	mColorbuffer;
	unsigned int	mColorbufferCount;
	
	bool			mDepthTexture;
	unsigned int	mDepthBuffer;
	
};

class Depthbuffer
{
public:
	Depthbuffer(unsigned int width, unsigned int height);
	~Depthbuffer();
	
	/// \name Write control
	/// \{
	/// Binds this depthbuffer for writing
	void bind();
	/// Returns to regular rendering
	void disable();
	/// \}
	
	/// \name Member access
	/// \{
	unsigned int getBuffer() const;
	
	unsigned int getWidth() const;
	unsigned int getHeight() const;
	/// \}

private:
	unsigned int	mBuffer;
	unsigned int	mDepthTexture;
	unsigned int	mWidth, mHeight;
	
};



inline unsigned int Framebuffer::getColorbufferCount() const
{
	return mColorbufferCount;
}

inline unsigned int Framebuffer::getColorbuffer(unsigned int i) const
{
	if (i >= mColorbufferCount)
		throw std::out_of_range("Index exceeds number of color buffers");
	
	return mColorbuffer[i];
}

inline unsigned int Framebuffer::getDepthbuffer() const 
{
	return mDepthBuffer;
}

inline unsigned int Framebuffer::getWidth() const
{
	return mWidth;
}

inline unsigned int Framebuffer::getHeight() const
{
	return mHeight;
}

inline GLenum Framebuffer::getMinFilterType() const
{
	return mFilter;
}

inline unsigned int Depthbuffer::getBuffer() const
{
	return mDepthTexture;
}

inline unsigned int Depthbuffer::getWidth() const
{
	return mWidth;
}

inline unsigned int Depthbuffer::getHeight() const
{
	return mHeight;
}



#endif
