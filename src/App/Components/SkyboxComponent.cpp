#include "../../Engine/ECS/Component.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "../../../include/glm/glm.hpp"
#include "../../../include/glm/gtc/matrix_transform.hpp"

#include <unordered_map>
#include <queue>

#include "SkyboxComponent.h"
#include "../Systems.h"
#include "../Systems/SkyboxRenderSystem.h"
#include "../AppRenderer.h"
#include "../ResourceLoader.h"
#include "../../Engine/Renderer.h"

DEFINE_COMPONENT(SkyboxComponent)

SkyboxComponent::SkyboxComponent() :
	rightTexPath(), leftTexPath(), topTexPath(), botTexPath(), frontTexPath(), backTexPath()
{
}

void SkyboxComponent::Init()
{
}

void SkyboxComponent::Exit()
{
}

void SkyboxComponent::Load()
{
	GetTexture(GetResourceLoader(), rightTexPath, &rightTex);
	GetTexture(GetResourceLoader(), leftTexPath, &leftTex);
	GetTexture(GetResourceLoader(), topTexPath, &topTex);
	GetTexture(GetResourceLoader(), botTexPath, &botTex);
	GetTexture(GetResourceLoader(), frontTexPath, &frontTex);
	GetTexture(GetResourceLoader(), backTexPath, &backTex);

	ResourceDescriptor* pSkyboxResDesc = nullptr;
	GetAppRenderer()->GetResourceDescriptorByName("Skybox", &pSkyboxResDesc);

	Renderer* pRenderer = GetAppRenderer()->GetRenderer();
	pSkyboxDescriptorSet = new DescriptorSet();
	pSkyboxDescriptorSet->desc = { pSkyboxResDesc, DescriptorUpdateFrequency::SET_2, 1 };
	CreateDescriptorSet(pRenderer, &pSkyboxDescriptorSet);

	const char* skyboxSamplerNames[6] = {
		"rightSampler",
		"leftSampler",
		"topSampler",
		"botSampler",
		"frontSampler",
		"backSampler"
	};
	Texture* pTextures[6] = {
		rightTex,
		leftTex,
		topTex,
		botTex,
		frontTex,
		backTex
	};

	DescriptorUpdateInfo descUpdateInfos[6] = {};
	for (uint32_t i = 0; i < 6; ++i)
	{
		descUpdateInfos[i].name = skyboxSamplerNames[i];
		descUpdateInfos[i].mImageInfo.imageView = pTextures[i]->imageView;
		descUpdateInfos[i].mImageInfo.imageLayout = pTextures[i]->desc.initialLayout;
		descUpdateInfos[i].mImageInfo.sampler = pRenderer->defaultResources.defaultSampler.sampler;
	}
	UpdateDescriptorSet(pRenderer, 0, pSkyboxDescriptorSet, 6, descUpdateInfos);

	GetAppRenderer()->GetModelMatrixFreeIndex("Skybox", &modelMatrixIndexInBuffer);
	GetSkyboxRenderSystem()->AddSkyboxComponent(this);
}

void SkyboxComponent::Unload()
{
	DestroyDescriptorSet(GetAppRenderer()->GetRenderer(), &pSkyboxDescriptorSet);
	delete pSkyboxDescriptorSet;

	GetSkyboxRenderSystem()->RemoveSkyboxComponent(this);
	GetAppRenderer()->RevokeModelMatrixIndex("Skybox", modelMatrixIndexInBuffer);


	rightTex = nullptr;
	leftTex = nullptr;
	topTex = nullptr;
	botTex = nullptr;
	frontTex = nullptr;
	botTex = nullptr;
}


START_REGISTRATION(SkyboxComponent)

DEFINE_VARIABLE(SkyboxComponent, rightTexPath)
DEFINE_VARIABLE(SkyboxComponent, leftTexPath)
DEFINE_VARIABLE(SkyboxComponent, topTexPath)
DEFINE_VARIABLE(SkyboxComponent, botTexPath)
DEFINE_VARIABLE(SkyboxComponent, frontTexPath)
DEFINE_VARIABLE(SkyboxComponent, backTexPath)


START_REFERENCES(SkyboxComponent)
	REFERENCE_STRING_VARIABLE(SkyboxComponent, rightTexPath)
	REFERENCE_STRING_VARIABLE(SkyboxComponent, leftTexPath)
	REFERENCE_STRING_VARIABLE(SkyboxComponent, topTexPath)
	REFERENCE_STRING_VARIABLE(SkyboxComponent, botTexPath)
	REFERENCE_STRING_VARIABLE(SkyboxComponent, frontTexPath)
	REFERENCE_STRING_VARIABLE(SkyboxComponent, backTexPath)
END