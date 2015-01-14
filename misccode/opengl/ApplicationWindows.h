#ifndef WINDOWS_APPLICATION__H_
#define WINDOWS_APPLICATION__H_

class WindowsApplication: public Application {
public:
    WindowsApplication();
    ~WindowsApplication();

    void Tick( int ms );

    void PumpMessages();

private:
    void InitMainWindow();

    HINSTANCE m_hInstance;

    int m_clientWidth;
    int m_clientHeight;
};

#endif /* WINDOWS_APPLICATION__H_ */
