
Application::Application() {
    m_lastTickTime = idLib::Milliseconds();
}

int Application::ElapsedTickMs() const {
    int lastTickTime = m_lastTickTime;
    m_lastTickTime = idLib::Milliseconds();
    return m_lastTickTime - lastTickTime;
}

void Application::OnKeyDown( KeyButtonKey k ) {
    switch ( k ) {
        case KeyButtonKey::BUTTON_W:
            gl_camera.MoveForward();
            break;

        case KeyButtonKey::BUTTON_S:
            gl_camera.MoveBackward();
            break;

        case KeyButtonKey::BUTTON_A:
            gl_camera.StrafeLeft();
            break;

        case KeyButtonKey::BUTTON_D:
            gl_camera.StrafeRight();
            break;
    };
}

void Application::OnKeyUp( KeyButtonKey k ) {
}

void Application::OnMouseDown( MouseButtonKey k ) {

}

void Application::OnMouseMove( const MouseEvent &ev ) {

}
