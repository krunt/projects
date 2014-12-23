
class LinuxApplication: public Application {
public:
    LinuxApplication();
    ~LinuxApplication();

    void Tick( int ms );

    void PumpMessages();
};
