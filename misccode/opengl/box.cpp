

int main() {

    std::shared_ptr<Application> app = CreateApplication();

    while ( true ) {
        app->PumpMessages();
        app->Tick( app->ElapsedTickMs() );
    }

    return 0;
}
