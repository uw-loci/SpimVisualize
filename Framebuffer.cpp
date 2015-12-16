/*
	OpenGL Frambuffer Object (FBO) abstraction. Includes fp16 framebuffers

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
#include "Framebuffer.h"

#include <vector>
#include <cassert>
#include <iostream>
#include <stdexcept>

/// validates the framebuffer configuration
static void validate(unsigned int buffer)
{
	glBindFramebuffer( GL_FRAMEBUFFER, buffer );
	GLenum err = glCheckFramebufferStatus( GL_FRAMEBUFFER );
	if (err != GL_FRAMEBUFFER_COMPLETE)
	{
		switch (err)
		{
			case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
				throw(std::runtime_error("Incomplete framebuffer"));
				break;
			case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
				throw(std::runtime_error("Incomplete drawing buffers"));
				break;
			case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
				throw(std::runtime_error("Missing attachments"));
				break;
			case GL_FRAMEBUFFER_UNSUPPORTED:
				throw(std::runtime_error("Unsupported"));
				break;
			default:
				throw(std::runtime_error("Unknown error"));
		}
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

Framebuffer::Framebuffer(unsigned int width, unsigned int height, GLenum internalFormat, GLenum type, unsigned int colorbufferCount, GLenum filter, bool depthTexture) :
	mWidth(width), mHeight(height), mInternalFormat(internalFormat), mType(type), mFilter(filter), mColorbufferCount(colorbufferCount), mDepthBuffer(0), mDepthTexture(depthTexture)
{	
	glGenFramebuffers(1, &mBuffer);
	glBindFramebuffer( GL_FRAMEBUFFER, mBuffer );

	mColorbuffer = new unsigned int[mColorbufferCount];
	

	//mChannels = getChannelConfig(channels, type);

	// create the color buffers
	glGenTextures( mColorbufferCount, mColorbuffer );
	for (unsigned int i = 0; i < mColorbufferCount; ++i)
	{		
		glBindTexture( GL_TEXTURE_2D, mColorbuffer[i] );
		
				
		glTexImage2D(GL_TEXTURE_2D, 0, mInternalFormat, mWidth, mHeight, 0, GL_RGBA, type, 0);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP); 
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
		
		static float maximumAnisotropy = -1.f;
		if (maximumAnisotropy < 0.f)
		{
			glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maximumAnisotropy);
			//std::clog << "Maximum tex anisotropy level: " << maximumAnisotropy << std::endl;
		}
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, maximumAnisotropy);
		
		//glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );		
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, mColorbuffer[i], 0);
	}
	
	
	if (mDepthTexture)
	{
	
		glGenTextures(1, &mDepthBuffer);
		glBindTexture(GL_TEXTURE_2D, mDepthBuffer);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, mWidth, mHeight, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, 0);
		//glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, mWidth, mHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
		glBindTexture(GL_TEXTURE_2D, 0);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, mDepthBuffer, 0);
	
	}
	else
	{
		// create the depth buffer
		glGenRenderbuffers( 1, &mDepthBuffer );
		glBindRenderbuffer( GL_RENDERBUFFER, mDepthBuffer );
		glRenderbufferStorage( GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mWidth, mHeight );
		glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, mDepthBuffer );
	}
	
	validate(mBuffer);
	glBindFramebuffer( GL_FRAMEBUFFER, 0 );
}	

Framebuffer::~Framebuffer()
{
	glDeleteTextures( mColorbufferCount, mColorbuffer );
	delete[] mColorbuffer;
	
	if (mDepthTexture)
		glDeleteTextures(1, &mDepthBuffer);
	else
		glDeleteRenderbuffers(1, &mDepthBuffer);
	glDeleteFramebuffers(1, &mBuffer);
}

void Framebuffer::bind()
{
	glBindFramebuffer( GL_FRAMEBUFFER, mBuffer );
	glPushAttrib(GL_VIEWPORT_BIT);
	glViewport(0, 0, mWidth, mHeight );
	
	if (mColorbufferCount > 1)
	{
		std::vector<GLenum> buffers(mColorbufferCount);
		for (unsigned int i = 0; i < mColorbufferCount; ++i)
			buffers[i] = GL_COLOR_ATTACHMENT0 + i;
		
		glDrawBuffers(mColorbufferCount, &buffers[0]);
	}
	else
	{
		glDrawBuffer(GL_COLOR_ATTACHMENT0);
	}
}


void Framebuffer::disable()
{
	// update mipmap
	if (mFilter == GL_LINEAR_MIPMAP_LINEAR || mFilter == GL_LINEAR_MIPMAP_NEAREST || mFilter == GL_NEAREST_MIPMAP_LINEAR || mFilter == GL_NEAREST_MIPMAP_NEAREST)
	{
		for (unsigned int i = 0; i < mColorbufferCount; ++i)
		{
			glBindTexture(GL_TEXTURE_2D, mColorbuffer[i]);
			glGenerateMipmap( GL_TEXTURE_2D );
		}
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	
	glPopAttrib();//GL_VIEWPORT_BIT);
	glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	//glDrawBuffer(GL_BACK);
	
}	
	
Depthbuffer::Depthbuffer(unsigned int width, unsigned int height) : 
	mWidth(width), mHeight(height)
{
	// create the depth texture
	glGenTextures(1, &mDepthTexture);
	glBindTexture(GL_TEXTURE_2D, mDepthTexture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, mWidth, mHeight, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, 0);
	glBindTexture(GL_TEXTURE_2D, 0);

	// create the fbo
	glGenFramebuffers(1, &mBuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, mBuffer);

	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, mDepthTexture, 0);
	
	validate(mBuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

Depthbuffer::~Depthbuffer()
{
	glDeleteTextures(1, &mDepthTexture);
	glDeleteFramebuffers(1, &mBuffer);
}

void Depthbuffer::bind()
{
	glBindFramebuffer( GL_FRAMEBUFFER, mBuffer );
	glDrawBuffer( GL_NONE );
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
	
	glPushAttrib(GL_VIEWPORT_BIT);
	glViewport(0, 0, mWidth, mHeight);
}

void Depthbuffer::disable()
{
	glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	glDrawBuffer(GL_BACK);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	
	glPopAttrib();
}

