
FMyRtmpConnection::FMyRtmpConnection( const FLiveStreamInfo &Info )
    : StreamInfo( Info ), Active( false )
{}

FMyRtmpConnection::~FMyRtmpConnection() {
    Shutdown();
}

bool FMyRtmpConnection::Start( const FGameBroadcastConfig &Config ) {
    BroadcastConfig = Config;

    av_register_all();

    AvOutFormat = av_guess_format("flv", NULL, NULL);

    if (!AvOutFormat) {
        return false;
    }

    AvFormatContext = avformat_alloc_context();
    if ( !AvFormatContext ) {
        return false;
    }

    FString Filename = StreamInfo.URL;
    Filename += TEXT( "/" );
    Filename += StreamInfo.StreamName;

    AvFormatContext->oformat = AvOutFormat;
    snprintf( AvFormatContext->filename, sizeof(AvFormatContext->filename), "%s", 
            (const char *)*Filename );

    if (AvOutFormat->video_codec == AV_CODEC_ID_NONE) {
        return false;
    }

    if ( !AddVideoStream() ) {
        return false;
    }

    if ( !OpenVideo() ) {
        return false;
    }

    av_dump_format( AvOutFormat, 0, AvFormatContext->filename, 1 );

    av_write_header( AvFormatContext, NULL );

    return true;
}

bool FMyRtmpConnection::AddVideoStream() {
    LibAVOutputStream *ost = &AvStream;
    AVCodecContext *c;
    AVCodec *codec;
    AVFormatContext *oc = AvFormatContext;
    
    /* find the video encoder */
    codec = avcodec_find_encoder(AvOutFormat->codec_id);
    if (!codec) {
        return false;
    }

    ost->st = avformat_new_stream( oc, codec );
    if (!ost->st) {
        return false;
    }

    c = ost->st->codec;

    /* Put sample parameters. */
    c->bit_rate = 400000;
    /* Resolution must be a multiple of two. */
    c->width    = BroadcastConfig.VideoBufferWidth;
    c->height   = BroadcastConfig.VideoBufferHeight;
    /* timebase: This is the fundamental unit of time (in seconds) in terms
     * of which frame timestamps are represented. For fixed-fps content,
     * timebase should be 1/framerate and timestamp increments should be
     * identical to 1. */
    ost->st->time_base = (AVRational){ 1, BroadcastConfig.FramesPerSecond };
    c->time_base       = ost->st->time_base;

    c->gop_size      = 12; /* emit one intra frame every twelve frames at most */
    c->pix_fmt       = AV_PIX_FMT_YUV420P;
    if (c->codec_id == AV_CODEC_ID_MPEG2VIDEO) {
        /* just for testing, we also add B frames */
        c->max_b_frames = 2;
    }
    if (c->codec_id == AV_CODEC_ID_MPEG1VIDEO) {
        /* Needed to avoid using macroblocks in which some coeffs overflow.
         * This does not happen with normal video, it just happens here as
         * the motion of the chroma plane does not match the luma plane. */
        c->mb_decision = 2;
    }
    /* Some formats want stream headers to be separate. */
    if (oc->oformat->flags & AVFMT_GLOBALHEADER)
        c->flags |= CODEC_FLAG_GLOBAL_HEADER;

    return true;
}

bool FMyRtmpConnection::OpenVideo() {
    AVCodecContext *c;

    c = AvStream.st->codec;

    /* open the codec */
    if (avcodec_open2( c, NULL, NULL ) < 0) {
        return false;
    }

    /* Allocate the encoded raw picture. */
    AvStream.frame = AllocPicture( c->pix_fmt, c->width, c->height );
    if (!ost->frame) {
        return false;
    }

    AvStream.tmp_frame = AllocPicture( AV_PIX_FMT_RGB24, 
            BroadcastConfig.VideoBufferWidth, BroadcastConfig.VideoBufferHeight );

    if ( !AvStream.tmp_frame) {
        return false;
    }

    return true;
}

void FMyRtmpConnection::ConvertFrameInternal( const FMyFrame &Frame ) {
    int ret, x, y;
    int width, height;
    AVFrame *frame;

    ret = av_frame_make_writable(pict);
    if (ret < 0)
        return;

    frame = AvStream->tmp_frame;
    width = BroadcastConfig.VideoBufferWidth;
    height = BroadcastConfig.VideoBufferHeight;

    for ( y = 0; y < height; ++y ) {
        for ( x = 0; x < width; ++x ) {

            FColor &color = Frame.Pixels[ y * height + x ];

            uint8 R, G, B, A;

            switch ( BroadcastConfig.PixelFormat ) {
            case FBroadcastConfig::EBroadcastPixelFormat::R8G8B8A8:
                R = color.R;
                G = color.G;
                B = color.B;
                A = color.A;
                break;

            case FBroadcastConfig::EBroadcastPixelFormat::B8G8R8A8:
                B = color.B;
                G = color.G;
                R = color.R;
                A = color.A;
                break;

            case FBroadcastConfig::EBroadcastPixelFormat::A8R8G8B8:
                A = color.A;
                R = color.R;
                G = color.G;
                B = color.B;
                break;

            case FBroadcastConfig::EBroadcastPixelFormat::A8B8G8R8:
                A = color.A;
                B = color.B;
                G = color.G;
                R = color.R;
                break;

            default:
                check(0);
                break;
            };


            *( pict->data[0] + y * pict->linesize[0] + 3 * x + 0 ) = R;
            *( pict->data[0] + y * pict->linesize[0] + 3 * x + 1 ) = G;
            *( pict->data[0] + y * pict->linesize[0] + 3 * x + 2 ) = B;
        }
    }
}

AVFrame *FMyRtmpConnection::ConvertFrame( const FMyFrame &Frame ) {
    AvStream *ost = &AvStream;
    AVCodecContext *c = ost->st->codec;

    if ( !ost->sws_ctx ) {
        ost->sws_ctx = sws_getContext( BroadcastConfig.VideoBufferWidth, 
                                       BroadcastConfig.VideoBufferHeight,
                                      AV_PIX_FMT_RGB24,
                                      c->width, c->height,
                                      c->pix_fmt,
                                      SCALE_FLAGS, NULL, NULL, NULL);
        if (!ost->sws_ctx) {
            return nullptr;
        }
    }

    ConvertFrameInternal();
    sws_scale(ost->sws_ctx, ost->tmp_frame->data, ost->tmp_frame->linesize,
              0, BroadcastConfig.VideoBufferHeight, ost->frame->data, ost->frame->linesize);

    return ost->frame;
}

bool FMyRtmpConnection::PushFrame( const FMyFrame &Frame ) {
    int ret;
    int GotPacket = 0;
    AVPacket Pkt = { 0 };
    AVFormatContext *oc = AvFormatContext;
    OutputStream *ost = &AvStream;
    AVCodecContext *c = ost->st->codec;

    AVFrame *LibAvFrame = ConvertFrame( Frame );

    if ( !LibAvFrame ) {
        return false;
    }

    av_init_packet(&Pkt);

    /* encode the image */
    ret = avcodec_encode_video2( c, &Pkt, LibAvFrame, &GotPacket );
    if (ret < 0) {
        return false;
    }

    if ( GotPacket ) {
        av_packet_rescale_ts( &Pkt, c->time_base, ost->st->time_base );
        Pkt.stream_index = ost->st->index;
        ret = av_write_frame( oc, &Pkt );
    }

    if ( ret != 0 ) {
        return false;
    }

    return (LibAvFrame || GotPacket) ? true : false;
}

bool FMyRtmpConnection::Shutdown() {
    if ( !IsActive() ) {
        return true;
    }

    av_write_trailer( AvOutFormat, &AvStream );
    CloseStream( AvOutFormat );
    avformat_free_context( AvOutFormat );

    return true;
}

void FRtmpLiveStreamingService::Clear() {
    FrameStartIndex = FrameEndIndex = 0;

    for ( int32 i = 0; i < ERtmpFrameCount; ++i ) {
        if ( Frames[ i ].Pixels ) {
            delete [] Frames[ i ].Pixels;
            Frames[ i ].Pixels = nullptr;
        }
    }

    Connections.Empty();
}

FFRtmpLiveStreamingService::RtmpLiveStreamingService() {
    BroadcastActive = 0;
}

void FRtmpLiveStreamingService::StartBroadcasting( const FBroadcastConfig& Config ) {
    if ( BroadcastActive ) {
        return;
    }

    ActiveConfig = Config;

    Clear();

    for ( auto s: DefaultStreams ) {
        TSharedPtr<FMyRtmpConnection> Conn( new FMyRtmpConnection( Info ) );
        if ( Conn->Start( Config ) ) {
            Connections.Push( Conn );
        }
    }

    if ( Connections.Num() > 0 ) {
        FPlatformAtomics::InterlockedIncrement( &BroadcastActive );
        StartBroadcastThread();
    }
}

void FRtmpLiveStreamingService::StopBroadcasting() {
    FPlatformAtomics::InterlockedDecrement( &BroadcastActive );
}

bool FRtmpLiveStreamingService::IsBroadcasting() const {
    return BroadcastActive != 0;
}

bool FRtmpLiveStreamingService::IsReadyForVideoFrames() const {
    bool IsReady = false;

    FramesLock.Lock();
    IsReady = FrameStartIndex != ( FrameEndIndex + 1 ) % ERtmpFrameCount;
    FramesLock.Unlock();

    return IsReady;
}

bool FRtmpLiveStreamingService::HasPendingVideoFrames() const {
    bool HasFrames = false;

    FramesLock.Lock();
    HasFrames = FrameStartIndex != FrameEndIndex;
    FramesLock.Unlock();

    return HasFrames;
}

void FRtmpLiveStreamingService::PushVideoFrame( const FColor* VideoFrameBuffer ) {
    if ( !IsReadyForVideoFrames() ) {
        return;
    }

    int32 NextFrameIndex = 0;
    {
        FramesLock.Lock();
        NextFrameIndex = FrameEndIndex;
        FramesLock.Unlock();
    }

    FMyFrame &frame = Frames[ NextFrameIndex ];

    if ( !frame.Pixels ) {
        int32 numPixels = ActiveConfig.VideoBufferWidth * ActiveConfig * VideoBufferHeight;
        frame.Pixels = new FColor[ numPixels ];
        memcpy( frame.Pixels, VideoFrameBuffer, numPixels * sizeof( FColor ) );
    }

    {
        FramesLock.Lock();
        FrameEndIndex++;
        FrameEndIndex %= ERtmpFrameCount;
        FramesLock.Unlock();
    }
}

class FMyFrameEncodeThreadRunnable: public FRunnable {
public:
    FMyFrameEncodeThreadRunnable( FRtmpLiveStreamingService &Service );

    bool Init() { return true; }
    void Exit() {}
    void Stop() {}
    uint32 Run();

private:
    FRtmpLiveStreamingService &RtmpService;
};

uint32 FMyFrameEncodeThreadRunnable::Run() {
    while ( RtmpService.IsBroadcasting() ) {

        if ( !RtmpService.HasPendingVideoFrames() ) {
            FPlatformProcess::Sleep( 10 );
            continue;
        }

        RtmpService.FeedNextFrame();
    }
    return 0;
}

void FRtmpLiveStreamingService::StartBroadcastThread() {
    BroadcastThreadRunnable = new FMyFrameEncodeThreadRunnable( *this );
    BroadcastThread = FRunnableThread::Create( BroadcastThreadRunnable,
            TCHAR( "RtmpLiveStreamHelper" ), true, false, 16 * 1024, TPri_Normal,
            FPlatformAffinity::GetPoolThreadMask() );
}

void FRtmpLiveStreamingService::StopBroadcastThread() {
}

void FRtmpLiveStreamingService::FeedNextFrame() {
    check(IsBroadcasting() && HasPendingVideoFrames());

    int32 NextFrameIndex = 0;

    FramesLock.Lock();
    NextFrameIndex = FrameStartIndex;
    FramesLock.Unlock();

    FMyFrame &Frame = Frames[ NextFrameIndex ];

    check( Frame.Pixels );

    for ( auto Conn: Connections ) {
        Conn->PushFrame( Frame );
    }

    FramesLock.Lock();
    FrameStartIndex++;
    FrameStartIndex %= ERtmpFrameCount;
    FramesLock.Unlock();
}

static int GetNextPowerOfTwo( int v ) {
    v = v | v >> 16;
    v = v | v >> 8;
    v = v | v >> 4;
    v = v | v >> 2;
    v = v | v >> 1;
    return v + 1;
}

void FRtmpLiveStreamingService::MakeValidVideoBufferResolution( int& VideoBufferWidth, int& VideoBufferHeight ) const 
{
    if ( !FMath::IsPowerOfTwo( VideoBufferWidth ) ) {
        VideoBufferWidth = GetNextPowerOfTwo( VideoBufferWidth );
    }

    if ( !FMath::IsPowerOfTwo( VideoBufferHeight ) ) {
        VideoBufferHeight = GetNextPowerOfTwo( VideoBufferHeight );
    }
}

void FRtmpLiveStreamingService::QueryBroadcastConfig( FBroadcastConfig& OutBroadcastConfig ) const {
    check( IsBroadcasting() );
    OutBroadcastConfig = ActiveConfig;
}

void FRtmpLiveStreamingService::QueryLiveStreams( const FString& GameName, 
            FQueryLiveStreamsCallback CompletionCallback )
{
    TArray<FLiveStreamInfo> Infos;
    for ( auto Conn: Connections ) {
        Infos.Push( Conn->GetLiveStreamInfo() );
    }

    CompletionCallback.ExecuteIfBound( Infos, true );
}
