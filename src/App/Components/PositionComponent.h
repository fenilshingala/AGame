#pragma once

DECLARE_COMPONENT(PositionComponent)
public:
	PositionComponent();
	virtual void Init() override;
	virtual void Exit() override;
	virtual void Load() override;
	virtual void Unload() override;

	float x, y, z;
	float scaleX, scaleY, scaleZ;
	float rotation, rotationAxisX, rotationAxisY, rotationAxisZ;
END



REGISTER_COMPONENT_CLASS(PositionComponent)
	REGISTER_VARIABLE(x)
	REGISTER_VARIABLE(y)
	REGISTER_VARIABLE(z)
	REGISTER_VARIABLE(scaleX)
	REGISTER_VARIABLE(scaleY)
	REGISTER_VARIABLE(scaleZ)
	REGISTER_VARIABLE(rotation)
	REGISTER_VARIABLE(rotationAxisX)
	REGISTER_VARIABLE(rotationAxisY)
	REGISTER_VARIABLE(rotationAxisZ)
END