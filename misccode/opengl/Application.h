#ifndef APPLICATION__H_
#define APPLICATION__H_

enum class KeyButtonKey {
    BUTTON_NONE,
    BUTTON_W,
    BUTTON_S,
    BUTTON_A,
    BUTTON_D,
};

enum class MouseButtonKey {
    BUTTON_NONE,
    BUTTON_LEFT,
    BUTTON_RIGHT,
};

struct MouseEvent {
    int m_mouseX;
    int m_mouseY;
};


class Application {
public:
    Application();
    virtual ~Application() {}

    virtual void Tick() = 0;

    virtual void PumpMessages() = 0;

    int ElapsedTickMs() const;

    virtual void OnKeyDown( KeyButtonKey k );
    virtual void OnKeyUp( KeyButtonKey k );
    virtual void OnMouseDown( MouseButtonKey k );
    virtual void OnMouseMove( const MouseEvent &ev );

private:
    mutable int m_lastTickTime;
};

#endif /* APPLICATION__H_ */
