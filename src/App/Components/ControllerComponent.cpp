#include "../../Engine/ECS/Component.h"

#include "ControllerComponent.h"
#include "../Systems.h"
#include "../Systems/MotionSystem.h"

DEFINE_COMPONENT(ControllerComponent)

ControllerComponent::ControllerComponent() :
	playerControl(false)
{
}

void ControllerComponent::Init()
{}

void ControllerComponent::Exit()
{}

void ControllerComponent::Load()
{
	GetMotionSystem()->AddControllerComponent(this);
}

void ControllerComponent::Unload()
{
	GetMotionSystem()->RemoveControllerComponent(this);
}


START_REGISTRATION(ControllerComponent)

DEFINE_VARIABLE(ControllerComponent, playerControl)


START_REFERENCES(ControllerComponent)
	REFERENCE_VARIABLE(ControllerComponent, playerControl)
END