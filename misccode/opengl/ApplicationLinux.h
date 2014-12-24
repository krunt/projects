#ifndef LINUX_APPLICATION__H_
#define LINUX_APPLICATION__H_

class LinuxApplication: public Application {
public:
    LinuxApplication();
    ~LinuxApplication();

    void Tick( int ms );

    void PumpMessages();
};

#endif /* LINUX_APPLICATION__H_ */
