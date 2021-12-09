#pragma once

struct Texture;
struct DescriptorSet;

DECLARE_COMPONENT(SkyboxComponent)
public:
	SkyboxComponent();
	virtual void Init() override;
	virtual void Exit() override;
	virtual void Load() override;
	virtual void Unload() override;

	inline uint32_t GetModelMatrixIndexInBuffer() { return modelMatrixIndexInBuffer; }

	char* rightTexPath;
	char* leftTexPath;
	char* topTexPath;
	char* botTexPath;
	char* frontTexPath;
	char* backTexPath;

	Texture* rightTex;
	Texture* leftTex;
	Texture* topTex;
	Texture* botTex;
	Texture* frontTex;
	Texture* backTex;

	DescriptorSet* pSkyboxDescriptorSet;

private:
	uint32_t modelMatrixIndexInBuffer;
END



REGISTER_COMPONENT_CLASS(SkyboxComponent)
	REGISTER_VARIABLE(rightTexPath)
	REGISTER_VARIABLE(leftTexPath)
	REGISTER_VARIABLE(topTexPath)
	REGISTER_VARIABLE(botTexPath)
	REGISTER_VARIABLE(frontTexPath)
	REGISTER_VARIABLE(backTexPath)
END