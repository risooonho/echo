#include "engine/core/math/Rect.h"
#include "engine/core/log/Log.h"
#include "interface/image/PixelFormat.h"
#include "interface/image/Image.h"
#include "interface/image/TextureLoader.h"
#include "GLESRenderBase.h"
#include "GLESRenderer.h"
#include "GLESTexture.h"
#include "GLESMapping.h"
#include <iostream>

namespace Echo
{
	GLESTexture2D::GLESTexture2D(const String& name)
		: Texture(name)
	{
		m_hTexture = 0;
	}

	GLESTexture2D::GLESTexture2D(TexType texType, PixelFormat pixFmt, Dword usage, ui32 width, ui32 height, ui32 depth,
							   ui32 numMipmaps, const Buffer &buff, bool bBak)
		: Texture(texType, pixFmt, usage, width, height, depth, numMipmaps, buff)
		, m_isUploadGPU(false)
	{
		m_hTexture = 0;

		create2D(pixFmt, usage, width, height, numMipmaps, buff);
	}
	
	// 析构函数
	GLESTexture2D::~GLESTexture2D()
	{
		EchoSafeDelete(m_memeryData, MemoryReader);

		unloadFromGPU();
		if (m_hTexture)
			OGLESDebug(glDeleteTextures(1, &m_hTexture));
	}
	
	bool GLESTexture2D::updateSubTex2D(ui32 level, const Rect& rect, void* pData, ui32 size)
	{
		if(level >= m_numMipmaps || !pData)
			return false;

		OGLESDebug(glBindTexture(GL_TEXTURE_2D, m_hTexture));

		GLenum glFmt = GLES2Mapping::MapFormat(m_pixFmt);
		GLenum glType = GLES2Mapping::MapDataType(m_pixFmt);
		OGLESDebug(glTexSubImage2D(GL_TEXTURE_2D, level, (GLint)rect.left, (GLint)rect.top, (GLsizei)rect.getWidth(), (GLsizei)rect.getHeight(), glFmt, glType, pData));

		OGLESDebug(glBindTexture(GL_TEXTURE_2D, 0));

		return true;
	}

	bool GLESTexture2D::create2D(PixelFormat pixFmt, Dword usage, ui32 width, ui32 height, ui32 numMipmaps, const Buffer& buff)
	{
		for (ui32 level = 0; level < m_numMipmaps; ++level)
		{
			ui32 curMipSize = PixelUtil::CalcLevelSize(width, height, 1, level, pixFmt);
			m_uploadedSize += curMipSize;
		}
		m_width = width;
		m_height = height;
		m_pixFmt = pixFmt;

		OGLESDebug(glGenTextures(1, &m_hTexture));
		if (!m_hTexture)
		{
			EchoLogError("Create GLES2Texture [%s] failed.", PixelUtil::GetPixelFormatName(pixFmt).c_str());
			return false;
		}

		OGLESDebug(glBindTexture(GL_TEXTURE_2D, m_hTexture));
		OGLESDebug(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));

		OGLESDebug(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
		OGLESDebug(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));

		ui32 texWidth = width;
		ui32 texHeight = height;
		Byte* pData = buff.getData();

		bool bHasData = (pData != NULL ? true : false);

		PixelFormat srcPixFmt = pixFmt;
		PixelFormat dstPixFmt = pixFmt;
		switch (pixFmt)
		{
			case PF_R8_UNORM:
			{
				srcPixFmt = PF_R8_UNORM;
				dstPixFmt = PF_RGBA8_UNORM;
			} break;
			default: break;
		}

		bool bRequiredConvert = (srcPixFmt == dstPixFmt ? false : true);
		for (ui32 level = 0; level < m_numMipmaps; ++level)
		{
			Byte* curMipData = 0;
			if (bHasData)
			{
				if (bRequiredConvert)
				{
					PixelBox srcBox(texWidth, texHeight, 1, srcPixFmt, pData);
					PixelBox dstBox(texWidth, texHeight, 1, dstPixFmt);
					dstBox.pData = ECHO_ALLOC_T(Byte, dstBox.getConsecutiveSize());

					PixelUtil::BulkPixelConversion(srcBox, dstBox);
					curMipData = (Byte*)dstBox.pData;
				}
				else
				{
					curMipData = pData;
				}
			}

			ui32 curMipSize = PixelUtil::CalcLevelSize(width, height, 1, level, pixFmt);
			GLenum internalFmt = GLES2Mapping::MapInternalFormat(dstPixFmt);
			if (PixelUtil::IsCompressed(pixFmt))
			{
				OGLESDebug(glCompressedTexImage2D(GL_TEXTURE_2D, level, internalFmt, texWidth, texHeight, 0, curMipSize, curMipData));
			}
			else
			{
				GLenum glFmt = GLES2Mapping::MapFormat(dstPixFmt);
				GLenum glType = GLES2Mapping::MapDataType(dstPixFmt);
				OGLESDebug(glTexImage2D(GL_TEXTURE_2D, level, internalFmt, texWidth, texHeight, 0, glFmt, glType, curMipData));
			}

			if (bHasData)
			{
				if (bRequiredConvert)
					ECHO_FREE(curMipData);
			}

			pData += curMipSize;

			texWidth = (texWidth > 1) ? texWidth >> 1 : 1;
			texHeight = (texHeight > 1) ? texHeight >> 1 : 1;
		}

		OGLESDebug(glBindTexture(GL_TEXTURE_2D, 0));
		return true;
	}

	void GLESTexture2D::unloadFromGPU()
	{
		if (m_hTexture)
		{
			OGLESDebug(glDeleteTextures(1, &m_hTexture));
			m_hTexture = 0;
		}

		if (m_isUploadGPU)
		{
			m_isUploadGPU = false;
		}
	}  

	bool GLESTexture2D::loadToGPU()
	{ 
		// texture has already loaded to gpu
		if (m_hTexture)
			return true;

		// no memory data, can't load to gpu
		if (!m_memeryData)
			return m_isUploadGPU ? true : false;

		// 满足加载条件，继续执行
		bool no_error = false;
		switch (m_compressType)
		{
		case Texture::CompressType_Unknown:
			no_error = _upload_common();
			break;
		}

		m_isUploadGPU = true;
		return no_error;
	}

	bool GLESTexture2D::createCube(PixelFormat pixFmt, Dword usage, ui32 width, ui32 height, ui32 numMipmaps, const Buffer& buff)
	{
		for (ui32 face = 0; face < 6; face++)
		{
			for (ui32 level = 0; level < m_numMipmaps; ++level)
			{
				m_uploadedSize += PixelUtil::CalcLevelSize(width, height, 1, level, pixFmt);;
			}
		}
		
		unsigned char* image[Texture::CF_End];
		unsigned char* pixel_data[Texture::CF_End];
		size_t offset = 0;

		ui32 texWidth = width;
		ui32 texHeight = height;
		Byte* pData = buff.getData();

		bool bHasData = (pData != NULL ? true : false);

		for (int i = 0; i < Texture::CF_End; i++)
		{
			image[i] = pData + offset;
			pixel_data[i] = (image[i] + sizeof(TGAHeaderInfo));
			offset += (m_memeryData->getSize() / 6);
		}

		OGLESDebug(glGenTextures(1, &m_hTexture));
		if (!m_hTexture)
		{
			EchoLogError("Create GLES2Texture [%s] failed.", PixelUtil::GetPixelFormatName(pixFmt).c_str());
			return false;
		}

		// generate gpu texture
		OGLESDebug(glBindTexture(GL_TEXTURE_CUBE_MAP, m_hTexture));
		OGLESDebug(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));

		// set default sampler state
		OGLESDebug(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
		OGLESDebug(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR));


		PixelFormat srcPixFmt = pixFmt;
		PixelFormat dstPixFmt = pixFmt;
		switch (pixFmt)
		{
			case PF_R8_UNORM:
			{
				srcPixFmt = PF_R8_UNORM;
				dstPixFmt = PF_RGBA8_UNORM;
			} break;
			case PF_RGBA8_UNORM:
			{
				srcPixFmt = PF_BGRA8_UNORM;
				dstPixFmt = PF_RGBA8_UNORM;
			} break;
			case PF_RGB8_UNORM:
			{
				srcPixFmt = PF_BGR8_UNORM;
				dstPixFmt = PF_RGB8_UNORM;
			} break;
			default: break;
		}

		GLenum internalFmt = GLES2Mapping::MapInternalFormat(dstPixFmt);
		GLenum glFmt = GLES2Mapping::MapFormat(dstPixFmt);
		GLenum glType = GLES2Mapping::MapDataType(dstPixFmt);

		bool bRequiredConvert = (srcPixFmt == dstPixFmt ? false : true);

		for (ui32 face = 0; face < 6; face++)
		{
			texWidth = width;
			texHeight = height;

			for (ui32 level = 0; level < numMipmaps; ++level)
			{

				Byte* curMipData = 0;

				if (bHasData)
				{
					if (bRequiredConvert)
					{
						PixelBox srcBox(texWidth, texHeight, 1, srcPixFmt, pixel_data[face]);
						PixelBox dstBox(texWidth, texHeight, 1, dstPixFmt);
						dstBox.pData = ECHO_ALLOC_T(Byte, dstBox.getConsecutiveSize());

						PixelUtil::BulkPixelConversion(srcBox, dstBox);
						curMipData = (Byte*)dstBox.pData;
					}
					else
					{
						curMipData = pixel_data[face];
					}
				}

				OGLESDebug(glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, level, internalFmt, texWidth, texHeight, 0, glFmt, glType, curMipData));

				if (bHasData)
				{
					if (bRequiredConvert)
						ECHO_FREE(curMipData);
				}

				ui32 curMipSize = PixelUtil::CalcLevelSize(width, height, 1, level, pixFmt);
				pData += curMipSize;

				texWidth = (texWidth > 1) ? texWidth >> 1 : 1;
				texHeight = (texHeight > 1) ? texHeight >> 1 : 1;
			}
		}

		// 暂强制
		//OGLESDebug(glGenerateMipmap(m_hTexture));

		OGLESDebug(glBindTexture(GL_TEXTURE_CUBE_MAP, 0));

		return true;
	}
}
