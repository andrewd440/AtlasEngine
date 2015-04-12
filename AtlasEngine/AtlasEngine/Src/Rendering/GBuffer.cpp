#include "Rendering\GBuffer.h"


GBuffer::GBuffer(const Vector2ui& TexResolution, const GLenum InternalFormat1)
	: mFrameBuffer(0)
	, mResolution(TexResolution)
{
	glGenFramebuffers(1, &mFrameBuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, mFrameBuffer);
	ASSERT(mFrameBuffer != 0);

	// Create targets with input parameters
	glGenTextures(2, mTextureIDs);
	ASSERT(mTextureIDs[0] != 0 && mTextureIDs[1] != 0);

	glBindTexture(GL_TEXTURE_2D, mTextureIDs[0]);
	glTexStorage2D(GL_TEXTURE_2D, 1, InternalFormat1, TexResolution.x, TexResolution.y);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	// Depth texture
	glBindTexture(GL_TEXTURE_2D, mTextureIDs[1]);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT32F, TexResolution.x, TexResolution.y);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	// Set attachments
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, mTextureIDs[0], 0);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, mTextureIDs[1], 0);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

GBuffer& GBuffer::operator=(GBuffer&& Other)
{
	mFrameBuffer = Other.mFrameBuffer;
	mTextureIDs[0] = Other.mTextureIDs[0];
	mTextureIDs[1] = Other.mTextureIDs[1];
	mResolution = Other.mResolution;

	Other.mFrameBuffer = 0;
	Other.mTextureIDs[0] = 0;
	Other.mTextureIDs[1] = 0;
	Other.mResolution = Vector2ui{ 0, 0 };

	return *this;
}

GBuffer::~GBuffer()
{
	glDeleteFramebuffers(1, &mFrameBuffer);
	glDeleteTextures(2, mTextureIDs);
}

void GBuffer::StartWrite()
{
	glBindFramebuffer(GL_FRAMEBUFFER, mFrameBuffer);
	glViewport(0, 0, mResolution.x, mResolution.y);

	static const GLenum DrawBuffers[] = { GL_COLOR_ATTACHMENT0 };
	static const GLuint uInt_Zeros[] = { 0, 0, 0, 0 };
	static const GLfloat Float_Ones[] = { 1.0f, 1.0f, 1.0f, 1.0f };

	glDrawBuffers(1, DrawBuffers);
	glClearBufferuiv(GL_COLOR, 0, uInt_Zeros);
	glClearBufferfv(GL_DEPTH, 0, Float_Ones);

}

void GBuffer::EndWrite()
{ 
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDrawBuffer(GL_BACK);
}

void GBuffer::StartRead()
{
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, mTextureIDs[0]);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, mTextureIDs[1]);
}

void GBuffer::EndRead()
{
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, 0);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);
}