#include "../Engine/OS/FileSystem.h"
#include "../Engine/ECS/EntityManager.h"
#include "../Engine/IApp.h"

class App : public IApp
{
public:
	void Init()
	{
		
	}

	void Exit()
	{}

	void Update()
	{}
};

APP_MAIN(App)