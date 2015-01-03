
#include "d3lib/Lib.h"

#include "GLTarget.h"
#include "GLTexture.h"

#include "Utils.h"


GLTarget glDefaultTarget( 0 );

void GLTarget::Init( int width, int height  ) {
    m_width = width;
    m_height = height;

    _CH( glGenTextures( 1, &m_renderTexture ));
    _CH( glActiveTexture( GL_TEXTURE0 + 50 ) );
    _CH( glBindTexture( GL_TEXTURE_2D, m_renderTexture ));
    _CH( glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, m_width, m_height, 0,
        GL_RGBA, GL_UNSIGNED_BYTE, (void*)NULL ));

    /*
    _CH(glGenRenderbuffers( 1, &m_renderBufferId ));
    _CH(glBindRenderbuffer( GL_RENDERBUFFER, m_renderBufferId ));
    _CH(glRenderbufferStorage( GL_RENDERBUFFER, GL_RGBA, m_width, m_height ));
    */

    _CH(glGenRenderbuffers( 1, &m_renderDepthBufferId ));
    _CH(glBindRenderbuffer( GL_RENDERBUFFER, m_renderDepthBufferId ));
    _CH(glRenderbufferStorage( GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, m_width, m_height ));

    _CH(glGenFramebuffers( 1, &m_frameBufferId ));
    _CH(glBindFramebuffer( GL_DRAW_FRAMEBUFFER, m_frameBufferId ));

    _CH(glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                GL_RENDERBUFFER, m_renderDepthBufferId ));

    _CH(glGenRenderbuffers( 1, &m_renderBufferId ));
    _CH(glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 
                m_renderTexture, 0 ));

    /*
    _CH(glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                GL_RENDERBUFFER, m_renderBufferId ));
                */

    GLenum fboStatus = glCheckFramebufferStatus( GL_FRAMEBUFFER );
    if ( fboStatus != GL_FRAMEBUFFER_COMPLETE ) {
        msg_failure0( "framebuffer is not complete\n" );
    }

    _CH(glBindFramebuffer( GL_FRAMEBUFFER, 0 ));
}

void GLTarget::Bind( void ) {
    _CH(glBindFramebuffer( GL_FRAMEBUFFER, m_frameBufferId ));
}

void GLTarget::CopyToTexture( const GLTexture &texture ) {
    _CH(glBindTexture( GL_TEXTURE_2D, texture.m_texture ));
    _CH(glCopyTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA,
        0, 0, m_width, m_height, 0 ));
}


GLTarget::~GLTarget() {
    if ( m_frameBufferId ) {
        _CH(glDeleteRenderbuffers( 1, &m_renderBufferId ));
        _CH(glDeleteRenderbuffers( 1, &m_renderDepthBufferId ));
        _CH(glDeleteFramebuffers( 1, &m_frameBufferId ));
    }
}
