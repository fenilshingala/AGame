#pragma once

struct AppModel;
struct DescriptorSet;
class PositionComponent;

DECLARE_COMPONENT(ModelComponent)
public:
	ModelComponent();
	virtual void Init() override;
	virtual void Exit() override;
	virtual void Load() override;
	virtual void Unload() override;

	inline AppModel* GetModel() { return pModel; }
	inline uint32_t GetModelMatrixIndexInBuffer() { return modelMatrixIndexInBuffer; }
	inline void SetPositionComponent(PositionComponent* a_pPositionComponent) { pPositionComponent = a_pPositionComponent; }
	inline PositionComponent* GetPositionComponent() { return pPositionComponent; }

	char* modelPath;

private:
	AppModel* pModel;
	uint32_t modelMatrixIndexInBuffer;
	PositionComponent* pPositionComponent;
END



REGISTER_COMPONENT_CLASS(ModelComponent)
	REGISTER_VARIABLE(modelPath)
END