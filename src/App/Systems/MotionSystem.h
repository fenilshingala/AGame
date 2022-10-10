#pragma once

class ControllerComponent;

class MotionSystem
{
public:
	MotionSystem();
	~MotionSystem();

	void Update(float dt);

	void AddControllerComponent(ControllerComponent* a_pModelComponent);
	void RemoveControllerComponent(ControllerComponent* a_pModelComponent);
};