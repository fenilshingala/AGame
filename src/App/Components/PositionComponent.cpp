#include "../../Engine/ECS/Component.h"

#include "PositionComponent.h"

DEFINE_COMPONENT(PositionComponent)

PositionComponent::PositionComponent() :
	x(0.0f), y(0.0f), z(0.0f), scaleX(0.0f), scaleY(0.0f), scaleZ(0.0f), rotation(0.0f),
	rotationAxisX(0.0f), rotationAxisY(0.0f), rotationAxisZ(0.0f)
{
}

void PositionComponent::Init()
{}

void PositionComponent::Exit()
{}

void PositionComponent::Load()
{}

void PositionComponent::Unload()
{}


START_REGISTRATION(PositionComponent)

DEFINE_VARIABLE(PositionComponent, x)
DEFINE_VARIABLE(PositionComponent, y)
DEFINE_VARIABLE(PositionComponent, z)
DEFINE_VARIABLE(PositionComponent, scaleX)
DEFINE_VARIABLE(PositionComponent, scaleY)
DEFINE_VARIABLE(PositionComponent, scaleZ)
DEFINE_VARIABLE(PositionComponent, rotation)
DEFINE_VARIABLE(PositionComponent, rotationAxisX)
DEFINE_VARIABLE(PositionComponent, rotationAxisY)
DEFINE_VARIABLE(PositionComponent, rotationAxisZ)


START_REFERENCES(PositionComponent)
	REFERENCE_VARIABLE(PositionComponent, x)
	REFERENCE_VARIABLE(PositionComponent, y)
	REFERENCE_VARIABLE(PositionComponent, z)
	REFERENCE_VARIABLE(PositionComponent, scaleX)
	REFERENCE_VARIABLE(PositionComponent, scaleY)
	REFERENCE_VARIABLE(PositionComponent, scaleZ)
	REFERENCE_VARIABLE(PositionComponent, rotation)
	REFERENCE_VARIABLE(PositionComponent, rotationAxisX)
	REFERENCE_VARIABLE(PositionComponent, rotationAxisY)
	REFERENCE_VARIABLE(PositionComponent, rotationAxisZ)
END