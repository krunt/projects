

#ifdef WINDOWS
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
	PSTR cmdLine, int showCmd)
#else
int main() 
#endif
{

    std::shared_ptr<Application> app = CreateApplication();

    while ( true ) {
        app->PumpMessages();
        app->Tick( app->ElapsedTickMs() );
    }

    return 0;
}
