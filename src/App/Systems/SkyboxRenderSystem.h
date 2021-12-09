#pragma once

class SkyboxComponent;

class SkyboxRenderSystem
{
public:
	SkyboxRenderSystem();
	~SkyboxRenderSystem();

	void Update();

	void AddSkyboxComponent(SkyboxComponent* a_pModelComponent);
	void RemoveSkyboxComponent(SkyboxComponent* a_pModelComponent);
};