#include <engine/core/io/IO.h>
#include <engine/core/io/DataStream.h>
#include <engine/core/Util/PathUtil.h>
#include <engine/core/Base/EchoDef.h>
#include "interface/Texture.h"
#include "interface/Renderer.h"
#include "engine/core/log/Log.h"
#include "engine/core/Math/EchoMathFunction.h"
#include "image/PixelFormat.h"
#include "image/Image.h"
#include "image/TextureLoader.h"
#include <iostream>

namespace Echo
{
	static map<ui32, Texture*>::type	g_globalTextures;

	Texture::Texture(const String& name)
		: Res(name)
		, m_texType(TT_2D)
		, m_pixFmt(PF_UNKNOWN)
		, m_usage(TU_DYNAMIC)
		, m_width(0)
		, m_height(0)
		, m_depth(1)
		, m_numMipmaps(1)
		, m_pixelsSize(0)
		, m_memeryData( nullptr)
		, m_compressType(CompressType_Unknown)
		, m_bCompressed(false)
		, m_faceNum(1)
		, m_endian(0)
		, m_xDimension(0)
		, m_yDimension(0)
		, m_zDimension(0)
		, m_bitsPerPixel(0)
		, m_blockSize(0)
		, m_headerSize(0)
		, m_uploadedSize(0)
		, m_samplerState(NULL)
	{
	}

	Texture::Texture(TexType texType, PixelFormat pixFmt, Dword usage, ui32 width, ui32 height, ui32 depth, ui32 numMipmaps, const Buffer& buff, bool bBak)
		: m_texType(texType)
		, m_pixFmt(pixFmt)
		, m_usage(usage)
		, m_width(width)
		, m_height(height)
		, m_depth(depth)
		, m_numMipmaps(numMipmaps)
		, m_pixelsSize(0)
		, m_memeryData(nullptr)
		, m_compressType(CompressType_Unknown)
		, m_bCompressed(false)
		, m_faceNum(1)
		, m_endian(0)
		, m_xDimension(0)
		, m_yDimension(0)
		, m_zDimension(0)
		, m_bitsPerPixel(0)
		, m_blockSize(0)
		, m_headerSize(0)
		, m_uploadedSize(0)
		, m_samplerState(NULL)
	{
		if (numMipmaps > MAX_MINMAPS)
		{
			m_numMipmaps = MAX_MINMAPS;
			EchoLogWarning("Over the max support mipmaps, using the max mipmaps num.");
		}
		else
		{
			m_numMipmaps = (numMipmaps > 0 ? numMipmaps : 1);
		}
	}

	Texture::~Texture()
	{
	}

	void Texture::bindMethods()
	{

	}

	// load
	Res* Texture::load(const ResourcePath& path)
	{
		if (IO::instance()->isResourceExists(path.getPath()))
		{
			Texture* texture = Renderer::instance()->createTexture(path.getPath());
			texture->loadToMemory();
			texture->loadToGPU();
			return texture;
		}

		return nullptr;
	}

	// get global texture
	Texture* Texture::getGlobal(ui32 globalTextureIdx)
	{
		auto it = g_globalTextures.find(globalTextureIdx);
		if (it != g_globalTextures.end())
			return it->second;

		return nullptr;
	}

	// set global texture
	void Texture::setGlobal(ui32 globalTextureIdx, Texture* texture)
	{
		g_globalTextures[globalTextureIdx] = texture;
	}

	bool Texture::create2D(PixelFormat pixFmt, Dword usage, ui32 width, ui32 height, ui32 numMipmaps, const Buffer& buff)
	{
		return false;
	}

	bool Texture::reCreate2D(PixelFormat pixFmt, Dword usage, ui32 width, ui32 height, ui32 numMipmaps, const Buffer& buff)
	{
		unload();
		create2D(pixFmt, usage, width, height, numMipmaps, buff);
		m_pixelsSize = PixelUtil::CalcSurfaceSize(width, height, m_depth, numMipmaps, pixFmt);

		return true;
	}

	// sampler state
	void Texture::setSamplerState(const SamplerState::SamplerDesc& desc)
	{
		m_samplerState = Renderer::instance()->getSamplerState(desc);
	}

	size_t Texture::calculateSize() const
	{
		// need repaird
		return (size_t)PixelUtil::CalcSurfaceSize(m_width, m_height, m_depth, m_numMipmaps, m_pixFmt);
	}

	bool Texture::decodeFromPVR()
	{
		bool isSoftDecode = false;

		m_bCompressed = false;
		m_compressType = Texture::CompressType_Unknown;

		Byte* pTextureData = m_memeryData->getData<Byte*>();

		PVRTextureHeaderV3* pHeader = (PVRTextureHeaderV3*)pTextureData;
		m_width = pHeader->u32Width;
		m_height = pHeader->u32Height;
		m_depth = pHeader->u32Depth;
		m_numMipmaps = 1;
		m_faceNum = pHeader->u32NumFaces;
		m_pixFmt = pvrformatMapping(pHeader->u64PixelFormat);
		switch (m_pixFmt)
		{
			case Echo::PF_ETC2_RGB:
			case Echo::PF_ETC1:
				m_pixFmt = PF_RGB8_UNORM;
				isSoftDecode = true;
				break;

			case Echo::PF_ETC2_RGBA:
				m_pixFmt = PF_RGBA8_UNORM;
				isSoftDecode = true;
				break;
			
			default:
				m_pixFmt = PF_UNKNOWN;
				break;
		}

		return isSoftDecode;
	}

	bool Texture::decodeFromKTX()
	{
		bool isSoftDecode = false;
		ui8* pTextureData = m_memeryData->getData<ui8*>();

		KTXHeader* pKtxHeader = (KTXHeader*)pTextureData;

		EchoAssert(pKtxHeader->m_endianness == cs_big_endian);

		// for compressed texture, glType and glFormat must equal to 'zero'
		EchoAssert(pKtxHeader->m_type == 0 && pKtxHeader->m_format == 0);

		m_compressType = CompressType_Unknown;
		m_bCompressed = false;

		m_width = pKtxHeader->m_pixelWidth;
		m_height = pKtxHeader->m_pixelHeight;
		m_depth = pKtxHeader->m_pixelDepth <= 0 ? 1 : pKtxHeader->m_pixelDepth;
		//m_numMipmaps = pKtxHeader->m_numberOfMipmapLevels;
		// 软解的时候只做了一层的mipmap
		m_numMipmaps = 1;
		m_faceNum = pKtxHeader->m_numberOfFaces;

		const ui32 GL_COMPRESSED_RGB8_ETC2 = 0x9274;
		const ui32 GL_COMPRESSED_SRGB8_ETC2 = 0x9275;
		const ui32 GL_COMPRESSED_RGBA8_ETC2_EAC = 0x9278;
		const ui32 GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC = 0x9279;
		const ui32 GL_ETC1_RGB8_OES = 0x8D64;

		switch (pKtxHeader->m_internalFormat)
		{
			case GL_COMPRESSED_RGB8_ETC2:
			case GL_COMPRESSED_SRGB8_ETC2:
			case GL_ETC1_RGB8_OES:
				m_pixFmt = PF_RGB8_UNORM;
				isSoftDecode = true;
				break;

			case GL_COMPRESSED_RGBA8_ETC2_EAC:
			case GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC:
				m_pixFmt = PF_RGBA8_UNORM;
				isSoftDecode = true;
				break;

			default:
				m_pixFmt = PF_UNKNOWN;
		}
		
		return isSoftDecode;
	}

	bool Texture::loadToMemory()
	{
		if (!m_memeryData)
		{
			m_memeryData = EchoNew(MemoryReader(getPath()));
			return _data_parser();
		}

		return true;
	}

	bool Texture::unload()
	{
		unloadFromGPU();

		return true;
	}

	bool Texture::_data_parser()
	{
		EchoAssert(m_memeryData);

		ui32* pIdentifier = (ui32 *)m_memeryData->getData<ui32*>();
		if (true)
		{
			// its png ,tga or jpg
			return _parser_common();
		}

		return false;
	}

	bool Texture::_parser_common()
	{
		Buffer commonTextureBuffer(m_memeryData->getSize(), m_memeryData->getData<ui8*>(), false);
		Image* pImage = Image::CreateFromMemory(commonTextureBuffer, Image::GetImageFormat(getPath()));
		if (!pImage)
		{
			return false;
		}

		m_bCompressed = false;
		m_compressType = Texture::CompressType_Unknown;
		PixelFormat pixFmt = pImage->getPixelFormat();

		if (ECHO_ENDIAN == ECHO_ENDIAN_LITTLE)
		{
			switch (pixFmt)
			{
				case PF_BGR8_UNORM:		pixFmt = PF_RGB8_UNORM;		break;
				case PF_BGRA8_UNORM:	pixFmt = PF_RGBA8_UNORM;	break;
				default:;
			}
		}

		m_width = pImage->getWidth();
		m_height = pImage->getHeight();
		m_depth = pImage->getDepth(); // 1
		m_pixFmt = pixFmt;
		m_numMipmaps = pImage->getNumMipmaps();
		if (m_numMipmaps == 0)
			m_numMipmaps = 1;

		m_pixelsSize = PixelUtil::CalcSurfaceSize(m_width, m_height, m_depth, m_numMipmaps, m_pixFmt);
		EchoSafeDelete(m_memeryData, MemoryReader);
		m_memeryData = EchoNew(MemoryReader((const char*)pImage->getData(), m_pixelsSize));

		EchoSafeDelete(pImage, Image);

		return true;
	}

	bool Texture::_upload_common()
	{
		//if (m_isCubeMap && m_texType == TT_CUBE)
		//{
		//	Buffer buff(m_memeryData->getSize(), m_memeryData->getData<ui8*>());
		//	return createCube(m_pixFmt, m_usage, m_width, m_height, m_numMipmaps, buff);
		//}
		//else
		{
			Buffer buff(m_pixelsSize, m_memeryData->getData<ui8*>());
			return create2D(m_pixFmt, m_usage, m_width, m_height, m_numMipmaps, buff);
		}
	}
}