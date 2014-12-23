#ifndef MYRENDER__H_
#define MYRENDER__H_

class MyRenderBase {
public:
    void Init( void );
    void Render( const playerView_t &view );
    virtual std::shared_ptr<gsurface_t> CacheSurface( const surf_t &s );
    void AddSurface( const glsurf_t &s ) {
        m_surfs.push_back( s );
    }
    void Shutdown( void );

private:
    virtual void OnPreRender( const playerView_t &view );
    virtual void RenderSurface( const std::shared_ptr<gsurface_t> &surf ) = 0;
    virtual void OnPostRender( const playerView_t &view );

    void SetupViewMatrix( const playerView_t &view );
    void SetupProjectionMatrix( const playerView_t &view );

    bool CreateStandardShaders( void );

    virtual bool CreateShaderPrograms( void ) = 0;


protected:
    std::shared_ptr<GTexture> m_whiteTexture;
    std::shared_ptr<GTexture> m_skyTexture;

    std::shared_ptr<GpuProgram> m_shaderProgram;
    std::shared_ptr<GpuProgram> m_skyProgram;

    material_t m_whiteMaterial;
    material_t m_skyMaterial;

    idVec4 m_eye; /* eye-position */
    idMat4 m_viewMatrix;
    idMat4 m_projectionMatrix;

    std::vector<glsurf_t> m_surfs;

    std::map<std::string, std::shared_ptr<material_t>> m_materialCache;
};

#endif /* MYRENDER__H_ */
