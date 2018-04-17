#include "Render/Material.h"
#include "engine/core/util/StringUtil.h"
#include "engine/core/Util/PathUtil.h"
#include "engine/core/Util/LogManager.h"
#include "engine/core/Util/Exception.h"
#include "Render/Renderer.h"
#include "PixelFormat.h"
#include "engine/core/io/IO.h"
#include "Render/MaterialDesc.h"
#include <thirdparty/pugixml/pugixml.hpp>

namespace Echo
{
	template<> HashNode<Material>** PtrMonitor<Material>::m_ptrs = NULL;
	template<> int PtrMonitor<Material>::m_hashcount = 0;
	template<> unsigned int PtrMonitor<Material>::m_maxhash = 10;

	// 构造函数
	Material::Material()
		: PtrMonitor()
		, m_name(StringUtil::BLANK)
		, m_queue( NULL)
		, m_pBlendState(NULL)
		, m_pDepthState(NULL)
		, m_pRasterizerState(NULL)
		, m_pShaderProgram(NULL)
		, m_VertexStrite(0)
	{
		for(size_t i=0; i<MAX_TEXTURE_SAMPLER; ++i)
			m_pSamplerState[i] = 0;

		m_arrTexSamplerState.reserve(MAX_TEXTURE_SAMPLER);
		m_arrTexSamplerState.resize(MAX_TEXTURE_SAMPLER);
	}

	// 析构函数
	Material::~Material()
	{
		free();
	}

	void Material::free()
	{
		EchoSafeDelete(m_pBlendState, BlendState);
		EchoSafeDelete(m_pDepthState, DepthStencilState);
		EchoSafeDelete(m_pRasterizerState, RasterizerState);
		for(size_t i=0; i<MAX_TEXTURE_SAMPLER; ++i)
			m_pSamplerState[i] = NULL;
		EchoSafeDelete(m_pShaderProgram, ShaderProgram);

		m_mapSamplerState.clear();

//#ifdef ECHO_EDITOR_MODE
		for (MapDefaultUniforms::iterator iter = m_defaultUniforms.begin(); iter != m_defaultUniforms.end(); ++iter)
		{
			EchoSafeDelete(iter->second, DefaultUniform);
		}
		m_defaultUniforms.clear();
//#endif // ECHO_EDITOR_MODE

	}

	// 加载
	bool Material::loadFromFile(const String& filename, const String& macros)
	{
		try 
		{
			free();

			m_name   = filename;
			m_shaderDesc.macros = macros;

			MemoryReader memReader(filename.c_str());
			if (!memReader.getData<char*>())
			{
				return false;
			}

			return loadFromContent(memReader.getData<char*>(), macros);
		}
		catch(bool)
		{
			free();
			EchoException("Material::loadFromFile: Fail to Parse Material file [%s].", filename.c_str());
			return false;
		}
	}

	bool Material::loadFromContent(const char* content, const String& macros)
	{
		MemoryReader memReader(content, strlen(content));

		return loadFromContent(memReader.getData<char*>(), macros);
	}

	// 从内容加载
	bool Material::loadFromContent(char* content, const String& macros)
	{
		pugi::xml_document doc;
		doc.load(content);

		pugi::xml_node rootNode = doc.first_child();
		if (!rootNode)
		{
			EchoLogError("The Material file content is invalid.");
			return false;
		}

		return loadShaderFrom(&rootNode);
	}

	// 加载着色器
	bool Material::loadShaderFrom(void* node)
	{
		pugi::xml_node* rootNode = static_cast<pugi::xml_node*>(node);
		try
		{
			pugi::xml_node vsNode = rootNode->child("vs");
			String vsSrc, psSrc;
			if (vsNode)
			{
				vsSrc = vsNode.text().as_string();
			}

			pugi::xml_node psNode = rootNode->child("ps");
			if (psNode)
			{
				psSrc = psNode.text().as_string();
			}

			pugi::xml_node pElementNode = rootNode->first_child();
			if (pElementNode)
			{
				do
				{
					String strName = pElementNode.name();
					if (strName == "Macro")
					{
						if (!loadMacro(&pElementNode))
						{
							throw false;
						}
					}
					else if (strName == "BlendState")
					{
						if (!loadBlendState(&pElementNode))
						{
							throw false;
						}
					}
					else if (strName == "RasterizerState")
					{
						if (!loadRasterizerState(&pElementNode))
						{
							throw false;
						}
					}
					else if (strName == "DepthStencilState")
					{
						if (!loadDepthStencilState(&pElementNode))
						{
							throw false;
						}
					}
					else if (strName == "SamplerState")
					{
						if( !loadSamplerState_Ext( &pElementNode ) )
						{
							throw false;
						}
					}

					else if( strName == "Texture" )
					{
						if( !loadTexture_Ext( &pElementNode ) )
						{
							throw false;
						}
					}
					else if ( strName == "DefaultUniformValue" )
					{
						if ( !loadDefaultUniform( &pElementNode ) )
						{
							throw false;
						}
					}

					pElementNode = pElementNode.next_sibling();
				}
				while(pElementNode);
			}

			// 执行着色器创建
			try
			{
				createShaderProgram( vsSrc, psSrc);
			}
			catch(Exception& e)
			{
				EchoLogError(e.getMessage().c_str());
				return false;
			}

			return true;
		}
		catch (bool)
		{
			free();
			return false;
		}
	}

	static bool MappingStringArrayIdx(const String* arry, int count, const String& value, int& idx)
	{
		for (size_t i = 0; i < count; i++)
		{
			if (value == arry[i])
			{
				idx = i;
				return true;
			}
		}

		return false;
	}

	bool Material::loadBlendState( void* pNode )
	{
		BlendState::BlendDesc blendDesc;
		pugi::xml_node* pSubNode = static_cast<pugi::xml_node*>(pNode);
		try
		{
			for( pugi::xml_node elementNode = pSubNode->first_child(); elementNode; elementNode=elementNode.next_sibling())
			{
				int idx = 0;
				String strName = elementNode.name();
				if(strName == "BlendEnable")
					blendDesc.bBlendEnable = elementNode.attribute("value").as_bool();
				else if (strName == "SrcBlend")
				{
					if (MappingStringArrayIdx(s_BlendFactor, BlendState::BF_MAX, elementNode.attribute("value").as_string(""), idx))
						blendDesc.srcBlend = (BlendState::BlendFactor)idx;
				}
				else if (strName == "DstBlend")
				{
					if (MappingStringArrayIdx(s_BlendFactor, BlendState::BF_MAX, elementNode.attribute("value").as_string(""), idx))
						blendDesc.dstBlend = (BlendState::BlendFactor)idx;
				}
				else if (strName == "BlendOP")
				{
					if (MappingStringArrayIdx(s_BlendOperation, 6, elementNode.attribute("value").as_string(""), idx))
						blendDesc.blendOP = (BlendState::BlendOperation)idx;
				}
				else if (strName == "SrcAlphaBlend")
				{
					if (MappingStringArrayIdx(s_BlendFactor, 6, elementNode.attribute("value").as_string(""), idx))
						blendDesc.srcAlphaBlend = (BlendState::BlendFactor)idx;
				}
				else if (strName == "DstAlphaBlend")
				{
					if (MappingStringArrayIdx(s_BlendFactor, 6, elementNode.attribute("value").as_string(""), idx))
						blendDesc.dstAlphaBlend = (BlendState::BlendFactor)idx;
				}
				else if (strName == "AlphaBlendOP")
				{
					if (MappingStringArrayIdx(s_BlendOperation, 6, elementNode.attribute("value").as_string(""), idx))
						blendDesc.alphaBlendOP = (BlendState::BlendOperation)idx;
				}
				else if (strName == "ColorWriteMask")
					blendDesc.colorWriteMask = MappingColorMask(elementNode.attribute("value").as_string(""));
				else if (strName == "A2CEnable")
					blendDesc.bA2CEnable = elementNode.attribute("value").as_bool();
				else if (strName == "IndependBlendEnable")
					blendDesc.bIndependBlendEnable = elementNode.attribute("value").as_bool();
				else if (strName == "BlendFactor")
					blendDesc.blendFactor = StringUtil::ParseColor(elementNode.attribute("value").as_string());
			}

			createBlendState(blendDesc);

			return true;
		}
		catch(bool)
		{
			free();
			return false;
		}
	}

	bool Material::loadMacro(void * pNode)
	{
		return true;
		//rapidxml::xml_node<>* pSubNode = static_cast<rapidxml::xml_node<>*>(pNode);
		//rapidxml::xml_node<>* itemNode = pSubNode->first_node();
		//String strName = pSubNode->name();
		//try
		//{
		//	while (itemNode)
		//	{
		//		String item = itemNode->name();
		//		if (item != "item") throw false;

		//		rapidxml::xml_attribute<>* nameAttr = itemNode->first_attribute();
		//		String name = nameAttr->name();
		//		if (name != "name") throw false;
		//		m_shaderDesc.macros = nameAttr->value();

		//		rapidxml::xml_attribute<>* functionAttr = nameAttr->next_attribute();
		//		String function = functionAttr->name();
		//		if (function != "function") throw false;
		//		m_shaderDesc.func = functionAttr->value();

		//		rapidxml::xml_attribute<>* paramAttr = functionAttr->next_attribute();
		//		String param = paramAttr->name();
		//		if (param != "param") throw false;
		//		m_shaderDesc.param = paramAttr->value();

		//		itemNode = itemNode->next_sibling();
		//	}

		//	return true;
		//}
		//catch (bool)
		//{
		//	free();
		//	return false;
		//}
	}

	bool Material::loadRasterizerState( void* pNode )
	{
		RasterizerState::RasterizerDesc rasterizerState;
		//rapidxml::xml_node<>* pSubNode = static_cast<rapidxml::xml_node<>*>(pNode);
		//try
		//{
		//	rapidxml::xml_node<>* pElementNode = pSubNode->first_node();
		//	while(pElementNode)
		//	{
		//		String strName = pElementNode->name();
		//		if (strName == "PolygonMode")
		//		{
		//			rapidxml::xml_attribute<>* pVarNode = pElementNode->first_attribute();
		//			String val(pVarNode->value());
		//			for (size_t i=0; i<3; ++i)
		//			{
		//				if (val == s_PolygonMode[i])
		//				{
		//					rasterizerState.polygonMode = (RasterizerState::PolygonMode)i;
		//					break;
		//				}
		//			}
		//		}
		//		else if (strName == "ShadeModel")
		//		{
		//			rapidxml::xml_attribute<>* pVarNode = pElementNode->first_attribute();
		//			String val(pVarNode->value());
		//			for (size_t i=0; i<2; ++i)
		//			{
		//				if (val == s_ShadeModel[i])
		//				{
		//					rasterizerState.shadeModel = (RasterizerState::ShadeModel)i;
		//					break;
		//				}
		//			}
		//		}
		//		else if (strName == "CullMode")
		//		{
		//			rapidxml::xml_attribute<>* pVarNode = pElementNode->first_attribute();
		//			String val(pVarNode->value());
		//			for (size_t i=0; i<2; ++i)
		//			{
		//				if (val == s_CullMode[i])
		//				{
		//					rasterizerState.cullMode = (RasterizerState::CullMode)i;
		//					break;
		//				}
		//			}
		//		}
		//		else if (strName == "FrontFaceCCW")
		//		{
		//			rapidxml::xml_attribute<>* pVarNode = pElementNode->first_attribute();
		//			rasterizerState.bFrontFaceCCW = StringUtil::ParseBool(String(pVarNode->value()));
		//		}
		//		else if (strName == "DepthBias")
		//		{
		//			rapidxml::xml_attribute<>* pVarNode = pElementNode->first_attribute();
		//			rasterizerState.depthBias = StringUtil::ParseFloat(String(pVarNode->value()));
		//		}
		//		else if (strName == "DepthBiasFactor")
		//		{
		//			rapidxml::xml_attribute<>* pVarNode = pElementNode->first_attribute();
		//			rasterizerState.depthBiasFactor = StringUtil::ParseFloat(String(pVarNode->value()));
		//		}
		//		else if (strName == "DepthClip")
		//		{
		//			rapidxml::xml_attribute<>* pVarNode = pElementNode->first_attribute();
		//			rasterizerState.bDepthClip = StringUtil::ParseBool(String(pVarNode->value()));
		//		}
		//		else if (strName == "Scissor")
		//		{
		//			rapidxml::xml_attribute<>* pVarNode = pElementNode->first_attribute();
		//			rasterizerState.bScissor = StringUtil::ParseBool(String(pVarNode->value()));
		//		}
		//		else if (strName == "Multisample")
		//		{
		//			rapidxml::xml_attribute<>* pVarNode = pElementNode->first_attribute();
		//			rasterizerState.bMultisample = StringUtil::ParseBool(String(pVarNode->value()));
		//		}
		//		pElementNode = pElementNode->next_sibling();
		//	}
			createRasterizerState(rasterizerState);
			return true;
		//}
		//catch(bool)
		//{
		//	free();
		//	return false;
		//}
	}

	bool Material::loadDepthStencilState( void* pNode )
	{
		DepthStencilState::DepthStencilDesc depthStencilState;
		//rapidxml::xml_node<>* pSubNode = static_cast<rapidxml::xml_node<>*>(pNode);
		//try
		//{
		//	rapidxml::xml_node<>* pElementNode = pSubNode->first_node();
		//	while(pElementNode)
		//	{
		//		String strName = pElementNode->name();
		//		if (strName == "DepthEnable")
		//		{
		//			rapidxml::xml_attribute<>* pVarNode = pElementNode->first_attribute();
		//			depthStencilState.bDepthEnable = StringUtil::ParseBool(String(pVarNode->value()));
		//		}
		//		else if (strName == "WriteDepth")
		//		{
		//			rapidxml::xml_attribute<>* pVarNode = pElementNode->first_attribute();
		//			depthStencilState.bWriteDepth = StringUtil::ParseBool(String(pVarNode->value()));
		//		}
		//		else if (strName == "DepthFunc")
		//		{
		//			rapidxml::xml_attribute<>* pVarNode = pElementNode->first_attribute();
		//			String val(pVarNode->value());
		//			for (size_t i=0; i<RenderState::CF_MAXNUM; ++i)
		//			{
		//				if (val == s_ComparisonFunc[i])
		//				{
		//					depthStencilState.depthFunc = (RenderState::ComparisonFunc)i;
		//					break;
		//				}
		//			}
		//		}
		//		else if (strName == "FrontStencilEnable")
		//		{
		//			rapidxml::xml_attribute<>* pVarNode = pElementNode->first_attribute();
		//			depthStencilState.bFrontStencilEnable = StringUtil::ParseBool(String(pVarNode->value()));
		//		}
		//		else if (strName == "FrontStencilFunc")
		//		{
		//			rapidxml::xml_attribute<>* pVarNode = pElementNode->first_attribute();
		//			String val(pVarNode->value());
		//			for (size_t i=0; i<RenderState::CF_MAXNUM; ++i)
		//			{
		//				if (val == s_ComparisonFunc[i])
		//				{
		//					depthStencilState.frontStencilFunc = (RenderState::ComparisonFunc)i;
		//					break;
		//				}
		//			}
		//		}
		//		else if (strName == "FrontStencilReadMask")
		//		{
		//			rapidxml::xml_attribute<>* pVarNode = pElementNode->first_attribute();
		//			depthStencilState.frontStencilReadMask = StringUtil::ParseUI16(String(pVarNode->value()));
		//		}
		//		else if (strName == "FrontStencilWriteMask")
		//		{
		//			rapidxml::xml_attribute<>* pVarNode = pElementNode->first_attribute();
		//			depthStencilState.frontStencilWriteMask = StringUtil::ParseUI16(String(pVarNode->value()));
		//		}
		//		else if (strName == "FrontStencilFailOP")
		//		{
		//			rapidxml::xml_attribute<>* pVarNode = pElementNode->first_attribute();
		//			String val(pVarNode->value());
		//			for (size_t i=0; i<DepthStencilState::SOP_MAX; ++i)
		//			{
		//				if (val == s_StencilOperation[i])
		//				{
		//					depthStencilState.frontStencilFailOP = (DepthStencilState::StencilOperation)i;
		//					break;
		//				}
		//			}
		//		}
		//		else if (strName == "FrontStencilDepthFailOP")
		//		{
		//			rapidxml::xml_attribute<>* pVarNode = pElementNode->first_attribute();
		//			String val(pVarNode->value());
		//			for (size_t i=0; i<DepthStencilState::SOP_MAX; ++i)
		//			{
		//				if (val == s_StencilOperation[i])
		//				{
		//					depthStencilState.frontStencilDepthFailOP = (DepthStencilState::StencilOperation)i;
		//					break;
		//				}
		//			}
		//		}
		//		else if (strName == "FrontStencilPassOP")
		//		{
		//			rapidxml::xml_attribute<>* pVarNode = pElementNode->first_attribute();
		//			String val(pVarNode->value());
		//			for (size_t i=0; i<DepthStencilState::SOP_MAX; ++i)
		//			{
		//				if (val == s_StencilOperation[i])
		//				{
		//					depthStencilState.frontStencilPassOP = (DepthStencilState::StencilOperation)i;
		//					break;
		//				}
		//			}
		//		}
		//		else if (strName == "FrontStencilRef")
		//		{
		//			rapidxml::xml_attribute<>* pVarNode = pElementNode->first_attribute();
		//			depthStencilState.frontStencilRef = StringUtil::ParseUI32(String(pVarNode->value()));
		//		}
		//		else if (strName == "BackStencilEnable")
		//		{
		//			rapidxml::xml_attribute<>* pVarNode = pElementNode->first_attribute();
		//			depthStencilState.bBackStencilEnable = StringUtil::ParseBool(String(pVarNode->value()));
		//		}
		//		else if (strName == "BackStencilFunc")
		//		{
		//			rapidxml::xml_attribute<>* pVarNode = pElementNode->first_attribute();
		//			String val(pVarNode->value());
		//			for (size_t i=0; i<RenderState::CF_MAXNUM; ++i)
		//			{
		//				if (val == s_ComparisonFunc[i])
		//				{
		//					depthStencilState.backStencilFunc = (RenderState::ComparisonFunc)i;
		//					break;
		//				}
		//			}
		//		}
		//		else if (strName == "BackStencilReadMask")
		//		{
		//			rapidxml::xml_attribute<>* pVarNode = pElementNode->first_attribute();
		//			depthStencilState.backStencilReadMask = StringUtil::ParseUI16(String(pVarNode->value()));
		//		}
		//		else if (strName == "BackStencilWriteMask")
		//		{
		//			rapidxml::xml_attribute<>* pVarNode = pElementNode->first_attribute();
		//			depthStencilState.backStencilWriteMask = StringUtil::ParseI16(String(pVarNode->value()));
		//		}
		//		else if (strName == "BackStencilFailOP")
		//		{
		//			rapidxml::xml_attribute<>* pVarNode = pElementNode->first_attribute();
		//			String val(pVarNode->value());
		//			for (size_t i=0; i<DepthStencilState::SOP_MAX; ++i)
		//			{
		//				if (val == s_StencilOperation[i])
		//				{
		//					depthStencilState.backStencilFailOP = (DepthStencilState::StencilOperation)i;
		//					break;
		//				}
		//			}
		//		}
		//		else if (strName == "BackStencilDepthFailOP")
		//		{
		//			rapidxml::xml_attribute<>* pVarNode = pElementNode->first_attribute();
		//			String val(pVarNode->value());
		//			for (size_t i=0; i<DepthStencilState::SOP_MAX; ++i)
		//			{
		//				if (val == s_StencilOperation[i])
		//				{
		//					depthStencilState.backStencilDepthFailOP = (DepthStencilState::StencilOperation)i;
		//					break;
		//				}
		//			}
		//		}
		//		else if (strName == "BackStencilPassOP")
		//		{
		//			rapidxml::xml_attribute<>* pVarNode = pElementNode->first_attribute();
		//			String val(pVarNode->value());
		//			for (size_t i=0; i<DepthStencilState::SOP_MAX; ++i)
		//			{
		//				if (val == s_StencilOperation[i])
		//				{
		//					depthStencilState.backStencilPassOP = (DepthStencilState::StencilOperation)i;
		//					break;
		//				}
		//			}
		//		}
		//		else if (strName == "BackStencilRef")
		//		{
		//			rapidxml::xml_attribute<>* pVarNode = pElementNode->first_attribute();
		//			depthStencilState.backStencilRef = StringUtil::ParseI32(String(pVarNode->value()));
		//		}
		//		pElementNode = pElementNode->next_sibling();
		//	}
			createDepthState(depthStencilState);
			return true;
		//}
		//catch(bool)
		//{
		//	free();
		//	return false;
		//}
	}

	bool Material::loadSamplerState_Ext( void* pNode )
	{
		//rapidxml::xml_node<>* pSubNode = static_cast<rapidxml::xml_node<>*>(pNode);
		//try
		//{
		//	rapidxml::xml_node<>* pElementNode = pSubNode->first_node();
		//	while(pElementNode)
		//	{
		//		String stageName = pElementNode->name();

		//		rapidxml::xml_node<>* pSubElementNode = pElementNode->first_node();

		//		SamplerState::SamplerDesc samplerState;

		//		while( pSubElementNode )
		//		{

		//			String strName = pSubElementNode->name();

		//			if (strName == "MinFilter")
		//			{
		//				rapidxml::xml_attribute<>* pVarNode = pSubElementNode->first_attribute();

		//				String val(pVarNode->value());
		//				for (size_t i=0; i<SamplerState::FO_MAX; ++i)
		//				{
		//					if (val == s_FilterOption[i])
		//					{
		//						samplerState.minFilter = (SamplerState::FilterOption)i;
		//						break;
		//					}
		//				}
		//			}
		//			else if (strName == "MagFilter")
		//			{
		//				rapidxml::xml_attribute<>* pVarNode = pSubElementNode->first_attribute();
		//				String val(pVarNode->value());
		//				for (size_t i=0; i<SamplerState::FO_MAX; ++i)
		//				{
		//					if (val == s_FilterOption[i])
		//					{
		//						samplerState.magFilter = (SamplerState::FilterOption)i;
		//						break;
		//					}
		//				}
		//			}
		//			else if (strName == "MipFilter")
		//			{
		//				rapidxml::xml_attribute<>* pVarNode = pSubElementNode->first_attribute();
		//				String val(pVarNode->value());
		//				for (size_t i=0; i<SamplerState::FO_MAX; ++i)
		//				{
		//					if (val == s_FilterOption[i])
		//					{
		//						samplerState.mipFilter = (SamplerState::FilterOption)i;
		//						break;
		//					}
		//				}
		//			}
		//			else if (strName == "AddrUMode")
		//			{
		//				rapidxml::xml_attribute<>* pVarNode = pSubElementNode->first_attribute();
		//				String val(pVarNode->value());
		//				for (size_t i=0; i<SamplerState::AM_MAX; ++i)
		//				{
		//					if (val == s_AddressMode[i])
		//					{
		//						samplerState.addrUMode = (SamplerState::AddressMode)i;
		//						break;
		//					}
		//				}
		//			}
		//			else if (strName == "AddrVMode")
		//			{
		//				rapidxml::xml_attribute<>* pVarNode = pSubElementNode->first_attribute();
		//				String val(pVarNode->value());
		//				for (size_t i=0; i<SamplerState::AM_MAX; ++i)
		//				{
		//					if (val == s_AddressMode[i])
		//					{
		//						samplerState.addrVMode = (SamplerState::AddressMode)i;
		//						break;
		//					}
		//				}
		//			}
		//			else if (strName == "AddrWMode")
		//			{
		//				rapidxml::xml_attribute<>* pVarNode = pSubElementNode->first_attribute();
		//				String val(pVarNode->value());
		//				for (size_t i=0; i<SamplerState::AM_MAX; ++i)
		//				{
		//					if (val == s_AddressMode[i])
		//					{
		//						samplerState.addrWMode = (SamplerState::AddressMode)i;
		//						break;
		//					}
		//				}
		//			}
		//			else if (strName == "MaxAnisotropy")
		//			{
		//				rapidxml::xml_attribute<>* pVarNode = pSubElementNode->first_attribute();
		//				samplerState.maxAnisotropy = StringUtil::ParseI8(String(pVarNode->value()));
		//			}
		//			else if (strName == "CmpFunc")
		//			{
		//				rapidxml::xml_attribute<>* pVarNode = pSubElementNode->first_attribute();
		//				String val(pVarNode->value());
		//				for (size_t i=0; i<RenderState::CF_MAXNUM; ++i)
		//				{
		//					if (val == s_ComparisonFunc[i])
		//					{
		//						samplerState.cmpFunc = (RenderState::ComparisonFunc)i;
		//						break;
		//					}
		//				}
		//			}
		//			else if (strName == "BorderColor")
		//			{
		//				rapidxml::xml_attribute<>* pVarNode = pSubElementNode->first_attribute();
		//				samplerState.borderColor = StringUtil::ParseColor(String(pVarNode->value()));
		//			}
		//			else if (strName == "MinLOD")
		//			{
		//				rapidxml::xml_attribute<>* pVarNode = pSubElementNode->first_attribute();
		//				samplerState.minLOD = StringUtil::ParseFloat(String(pVarNode->value()));
		//			}
		//			else if (strName == "MaxLOD")
		//			{
		//				rapidxml::xml_attribute<>* pVarNode = pSubElementNode->first_attribute();
		//				samplerState.maxLOD = StringUtil::ParseFloat(String(pVarNode->value()));
		//			}
		//			else if (strName == "MipLODBias")
		//			{
		//				rapidxml::xml_attribute<>* pVarNode = pSubElementNode->first_attribute();
		//				samplerState.mipLODBias = StringUtil::ParseFloat(String(pVarNode->value()));
		//			}

		//			pSubElementNode = pSubElementNode->next_sibling();
		//		}

		//		const SamplerState* pSS = Renderer::instance()->getSamplerState(samplerState);
		//		EchoAssert( pSS );
		//		m_mapSamplerState.insert( SamplerStateMap::value_type(stageName, pSS) );

		//		pElementNode = pElementNode->next_sibling();
		//	}

			return true;
		//}
		//catch(bool)
		//{
		//	free();
		//	return false;
		//}
	}

	bool Material::loadTexture_Ext( void* pNode )
	{
		pugi::xml_node* pTextureNode = static_cast<pugi::xml_node*>(pNode);
		try
		{
			//for(pugi::xml_node pTextureStageNode = pTextureNode->first_child(); pTextureStageNode; pTextureStageNode=pTextureStageNode.next_sibling())
			//{
			//	String stageName = pTextureStageNode->name();
			//
			//	if( stageName != "stage" ) throw false;

			//	rapidxml::xml_attribute<>* pStageNoAttribute = pTextureStageNode->first_attribute();

			//	String no_name = pStageNoAttribute->name();

			//	if( no_name != "no" ) throw false;

			//	int stage_no = StringUtil::ParseInt(pStageNoAttribute->value());

			//	rapidxml::xml_attribute<>* pStageSampAttribute = pStageNoAttribute->next_attribute();
			//
			//	String strSampler = pStageSampAttribute->name();

			//	if( strSampler != "sampler" ) throw false;

			//	String strSampValue = pStageSampAttribute->value();

			//	if( strSampValue == "" ) throw false;

			//	m_arrTexSamplerState[stage_no] = strSampValue;
			//	m_pSamplerState[stage_no] = getSamplerStateByTexStage(stage_no);

			//	pTextureStageNode = pTextureStageNode->next_sibling();
			//}

			return true;
		}
		catch(bool)
		{
			free();
			return false;
		}
	}

	void Material::createBlendState(BlendState::BlendDesc& desc)
	{
		EchoSafeDelete(m_pBlendState, BlendState);
		m_pBlendState = Renderer::instance()->createBlendState(desc);
	}

	void Material::createDepthState(DepthStencilState::DepthStencilDesc& desc)
	{
		EchoSafeDelete(m_pDepthState, DepthStencilState);
		m_pDepthState = Renderer::instance()->createDepthStencilState(desc);
	}

	void Material::createRasterizerState(RasterizerState::RasterizerDesc& desc)
	{
		EchoSafeDelete(m_pRasterizerState, RasterizerState);
		m_pRasterizerState = Renderer::instance()->createRasterizerState(desc);
	}

	// 创建着色器
	void Material::createShaderProgram(const String& vsContent, const String& psContent)
	{
		EchoSafeDelete(m_pShaderProgram, ShaderProgram);
		Shader::ShaderDesc vsDesc(m_shaderDesc);
		Renderer* pRenderer = Renderer::instance();
		Shader *pVertexShader = pRenderer->createShader(Shader::ST_VERTEXSHADER, vsDesc, vsContent.data(), vsContent.size());
		if(!pVertexShader)
		{
			String output = "Error in create vs file: ";
			EchoException(output.c_str());
		}

		Shader::ShaderDesc psDesc(m_shaderDesc);

		Shader *pPixelShader = pRenderer->createShader(Shader::ST_PIXELSHADER, psDesc, psContent.data(), psContent.size());
		if(!pPixelShader)
		{
			String output = "Error in create ps file: ";
			EchoException(output.c_str());
		}

		// create shader program
		m_pShaderProgram = pRenderer->createShaderProgram(this);
		m_pShaderProgram->attachShader(pVertexShader);
		m_pShaderProgram->attachShader(pPixelShader);
		m_pShaderProgram->linkShaders();
	}

	void Material::activeShader()
	{
		EchoAssert(m_pShaderProgram);
		EchoAssert(m_pBlendState);
		EchoAssert(m_pDepthState);
		EchoAssert(m_pRasterizerState);

		m_pShaderProgram->bind();
	}

	// 获取采样状态
	const SamplerState* Material::getSamplerState(int stage) const
	{
		EchoAssert(stage>=0 && stage<MAX_TEXTURE_SAMPLER);
		return m_pSamplerState[stage] ? m_pSamplerState[stage] : Renderer::instance()->getSamplerState(SamplerState::SamplerDesc());
	}

	// 获取采样状态
	const SamplerState* Material::getSamplerStateByTexStage( int stage )
	{
		EchoAssert( stage < 8 );
		SamplerStateMap::iterator fit = m_mapSamplerState.find(m_arrTexSamplerState[stage]);
		if( fit != m_mapSamplerState.end() )
		{
			return fit->second;
		}

		return Renderer::instance()->getSamplerState(SamplerState::SamplerDesc());
	}

#ifdef ECHO_EDITOR_MODE
	// 获取材质可选宏定义列表
	StringArray Material::getEnabledMacros(const String& matFileName, bool withEnabled /* = false */)
	{
		StringArray macros;

		try
		{
			MemoryReader memReader( matFileName.c_str());
			pugi::xml_document doc;
			doc.load(memReader.getData<char*>());

			pugi::xml_node rootNode = doc.first_child();
			if (rootNode)
			{
				pugi::xml_node shaderNode = rootNode.child("ShaderPrograme");
				if (shaderNode)
				{
					pugi::xml_node macrosNode = shaderNode.child("Macros");
					if (macrosNode)
					{
						pugi::xml_node macroNode = macrosNode.child("Macro");
						while (macroNode)
						{
							pugi::xml_attribute attribute = macroNode.attribute("value");
							if (attribute)
							{
								macros.push_back(attribute.as_string());
								if (withEnabled)
								{
									pugi::xml_attribute enabledAttribute = macroNode.attribute("default");
									if (enabledAttribute)
										macros.push_back(enabledAttribute.as_string());
									else
										macros.push_back("false");
								}
							}
								
							macroNode = macroNode.next_sibling("Macro");
						}
					}
				}
			}
		}
		catch (...)
		{
			EchoLogError( "getEnabledMacros failed...");
		}

		return macros;
	}
#endif

	Material::DefaultUniform::~DefaultUniform()
	{
		EchoSafeFree(value);
	}

	bool Material::loadDefaultUniform(void* pNode)
	{
		pugi::xml_node* pDefaultUniforms = static_cast<pugi::xml_node*>(pNode);
		try
		{
			for(pugi::xml_node pUniform = pDefaultUniforms->first_child(); pUniform; pUniform = pUniform.next_sibling())
			{
				String strNameValue = pUniform.attribute("name").as_string();
				String strTypeValue = pUniform.attribute("type").as_string();
				i32 count = pUniform.attribute("count").as_int();
				String strValueValue = pUniform.attribute("value").as_string();

				DefaultUniform* uniform = EchoNew(DefaultUniform);
				void* value = createDefaultUniformValue(strTypeValue, count, strValueValue, uniform->sizeInByte, uniform->type);
				if (value)
				{
					uniform->count = count;
					uniform->value = value;

					m_defaultUniforms[strNameValue] = uniform;
				}
				else
				{
					EchoSafeDelete(uniform, DefaultUniform);
				}
			}

			return true;
		}
		catch (bool)
		{
			free();
			return false;
		}
	}

	const Material::DefaultUniform* Material::getDefaultUniformValue(const String& name)
	{
		MapDefaultUniforms::iterator iter = m_defaultUniforms.find(name);
		if (iter != m_defaultUniforms.end())
		{
			return iter->second;
		}

		return NULL;
	}

	void* Material::createDefaultUniformValue(const String& strType, const i32 count, const String& strValue, ui32& outSize, ShaderParamType& outType)
	{
		// 解析字符串，数据分段
		StringArray valueStr = StringUtil::Split(strValue, ";");
		ui32 size = valueStr.size();
		EchoAssertX(size == count, "Material::createDefaultUniformValue");

		// 根据类型计算相关数据
		if (strType == "SPT_INT")
		{
			outType = SPT_INT;
			outSize = sizeof(i32) * count;
		}
		else if (strType == "SPT_FLOAT")
		{
			outType = SPT_FLOAT;
			outSize = sizeof(Real) * count;
		}
		else if (strType == "SPT_VEC2")
		{
			outType = SPT_VEC2;
			outSize = sizeof(Vector2) * count;
		}
		else if (strType == "SPT_VEC3")
		{
			outType = SPT_VEC3;
			outSize = sizeof(Vector3) * count;
		}
		else if (strType == "SPT_VEC4")
		{
			outType = SPT_VEC4;
			outSize = sizeof(Vector4) * count;
		}
		else if (strType == "SPT_MAT4")
		{
			outType = SPT_MAT4;
			outSize = 0;// sizeof(Matrix4) * count;
		}
		else if (strType == "SPT_TEXTURE")
		{
			outType = SPT_TEXTURE;
			outSize = 0;
		}
		else
		{
			outType = SPT_UNKNOWN;
		}

		// 分配内存数据
		void* value = nullptr;
		if (outSize)
		{
			value = (void*)EchoMalloc(outSize);

			// 赋值
			for (ui32 i = 0; i < size; ++i)
			{
				switch (outType)
				{
				case Echo::SPT_INT:  ((i32*)value)[i] = StringUtil::ParseInt(valueStr[i]);		break;
				case Echo::SPT_FLOAT:((float*)value)[i] = StringUtil::ParseFloat(valueStr[i]);	break;
				case Echo::SPT_VEC2: ((Vector2*)value)[i] = StringUtil::ParseVec2(valueStr[i]);	break;
				case Echo::SPT_VEC3: ((Vector3*)value)[i] = StringUtil::ParseVec3(valueStr[i]);	break;
				case Echo::SPT_VEC4: ((Vector4*)value)[i] = StringUtil::ParseVec4(valueStr[i]); break;
				}
			}
		}

		return value;
	}

	bool Material::hasMacro(const char* const macro) const
	{
		String fullMacro;
		fullMacro = "#define ";
		fullMacro += macro;
		fullMacro += "\n";
		int i = m_shaderDesc.macros.find(fullMacro.c_str());
		return i != String::npos;
	}
}
