#pragma once

#include <functional>
#include <string>
#include <vector>
#include <unordered_map>
#include <iostream>

typedef unsigned int uint32;

class ComponentRepresentation
{
public:
	virtual ~ComponentRepresentation() {}
	virtual uint32 getComponentID() const = 0;
	virtual const char* getComponentName() const = 0;
	virtual std::unordered_map<std::string, uint32> getVarNames() = 0;
	virtual void setVar(const char* name, void* value) = 0;
	virtual void setStr(const char* name, const char** value) = 0;
};

class Component;
typedef Component* (*ComponentGeneratorFctPtr)();

class ComponentRegistrar
{// singleton
	friend class Component;
public:
	ComponentRegistrar() {}
	~ComponentRegistrar() {}

	static ComponentRegistrar* getInstance();
	static void destroyInstance();
	static Component* GetComponent(const char* component);

private:
	//inline const std::unordered_map<uint32_t, ComponentGeneratorFctPtr>& getComponentGeneratorMap() { return componentGeneratorMap; }
	void mapInsertion(uint32_t, ComponentGeneratorFctPtr);
	std::unordered_map<uint32_t, ComponentGeneratorFctPtr> componentGeneratorMap;
	static ComponentRegistrar* instance;
};


class Component
{
public:
	virtual ~Component() {}
	virtual Component* clone() const = 0;
	virtual uint32 getType() const = 0;
	virtual ComponentRepresentation* createRepresentation() = 0;
	virtual void destroyRepresentation(ComponentRepresentation* pRep) = 0;
	static void instertIntoComponentGeneratorMap(uint32_t component_name, Component* (*func)());

	virtual void Init() = 0;
	virtual void Exit() = 0;

	inline uint32_t GetOwnerID() { return mOwnerID; }
	inline void SetOwnerID(uint32_t a_uID) { mOwnerID = a_uID; }

private:
	uint32_t mOwnerID;
};


// Unique ID
static uint32 m_VarIdCounter = 0;
static const uint32 generateUniqueID(std::string component_name, ComponentGeneratorFctPtr instance)
{
	Component::instertIntoComponentGeneratorMap((uint32_t)std::hash<std::string>{}(component_name), instance);
	return m_VarIdCounter++;
}

struct Variable
{
	void*		address;
	uint32_t	size;
};

#define DECLARE_COMPONENT(Component_) \
class Component_ : public Component { \
	public: \
		virtual Component_* clone() const override; \
		virtual uint32 getType() const override; \
		static  uint32 getTypeStatic(); \
		virtual ComponentRepresentation* createRepresentation() override; \
		virtual void destroyRepresentation(ComponentRepresentation* pRep) override; \
\
		static uint32 Component_##typeHash; \
		static Component* GenerateComponent();


#define END };


#define DEFINE_COMPONENT(Component_) \
	Component_* Component_::clone() const { return new Component_; } \
	uint32 Component_::getType() const { return Component_::getTypeStatic(); } \
	uint32 Component_::getTypeStatic() {  return Component_##typeHash; } \
	ComponentRepresentation* Component_::createRepresentation() { return new Component_##Representation(this); } \
	void Component_::destroyRepresentation(ComponentRepresentation* pRep) { pRep->~ComponentRepresentation(); delete pRep; } \
	uint32 Component_::Component_##typeHash = (uint32)(std::hash<std::string>{}(#Component_)); \
	Component* Component_::GenerateComponent() { return new Component_; }


#define REGISTER_COMPONENT_CLASS(Component_) \
class Component_##Representation : public ComponentRepresentation \
{ \
public:\
	Component_##Representation() {} \
	Component_##Representation(Component_* const comp) \
	{ \
		addReferences(comp); \
	} \
	\
	virtual uint32 getComponentID() const override { return ComponentID; }\
	virtual const char* getComponentName() const override { return #Component_; }\
	static const uint32 storeIDandName(std::string var_name, const uint32 id)\
	{\
		Component_##varNames.emplace(var_name, id); \
		return id; \
	}\
	std::unordered_map<std::string, uint32> getVarNames() { return Component_##Representation::Component_##varNames; } \
	\
	void setVar(const char* name, void* value) \
	{ \
		memcpy(Component_##metadata[std::string(name)].address, value, Component_##metadata[std::string(name)].size); \
	} \
	void setStr(const char* name, const char** value) \
	{ \
		*(Component_##StrMetadata[std::string(name)]) = (char*)malloc(sizeof(char) * (strlen(*value)+1)); \
		memcpy(*(Component_##StrMetadata[std::string(name)]), *value, sizeof(char) * (strlen(*value)+1)); \
	} \
	void addReferences(Component_* const component);	\
	static const uint32 ComponentID; \
	static uint32 Component_VarCounter; \
	static std::unordered_map<std::string, uint32> Component_##varNames; \
	static std::unordered_map<std::string, Variable> Component_##metadata; \
	static std::unordered_map<std::string, char**> Component_##StrMetadata;


#define START_REGISTRATION(Component_) \
	uint32 Component_##Representation::Component_VarCounter = 0; \
	const uint32 Component_##Representation::ComponentID = generateUniqueID((std::string)(#Component_), Component_::GenerateComponent); \
	std::unordered_map<std::string, uint32> Component_##Representation::Component_##varNames; \
	std::unordered_map<std::string, Variable> Component_##Representation::Component_##metadata; \
	std::unordered_map<std::string, char**> Component_##Representation::Component_##StrMetadata;

#define REGISTER_VARIABLE(x) \
static const uint32 x;

#define DEFINE_VARIABLE(Component_, x) \
const uint32 Component_##Representation::x = Component_##Representation::storeIDandName(#x, Component_VarCounter++);


#define START_REFERENCES(Component_) \
void Component_##Representation::addReferences(Component_* const component) \
{

#define REFERENCE_VARIABLE(Component_, x) \
Component_##metadata[#x].address = (void*)&(component->x); \
Component_##metadata[#x].size = (uint32_t)sizeof(component->x);

#define REFERENCE_STRING_VARIABLE(Component_, x) \
Component_##StrMetadata[#x] = &(component->x);