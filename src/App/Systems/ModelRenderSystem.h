#pragma once

class ModelComponent;

class ModelRenderSystem
{
public:
	ModelRenderSystem();
	~ModelRenderSystem();

	void Update();

	void AddModelComponent(ModelComponent* a_pModelComponent);
	void RemoveModelComponent(ModelComponent* a_pModelComponent);
};