#pragma once

DECLARE_COMPONENT(ControllerComponent)
public:
	ControllerComponent();
	virtual void Init() override;
	virtual void Exit() override;
	virtual void Load() override;
	virtual void Unload() override;

	bool playerControl;
END



REGISTER_COMPONENT_CLASS(ControllerComponent)
	REGISTER_VARIABLE(playerControl)
END