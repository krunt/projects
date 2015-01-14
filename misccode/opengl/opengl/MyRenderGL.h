

class MyRenderGL : public MyRenderBase {
public:
    std::shared_ptr<gsurface_t> CacheSurface( const surf_t &s );

private:

    void OnPreRender( const playerView_t &view );
    virtual void RenderSurface( const std::shared_ptr<gsurface_t> &surf );
    bool CreateShaderPrograms();

};

typedef MyRenderGL MyRender;
extern MyRender gl_render;
