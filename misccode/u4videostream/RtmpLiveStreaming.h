
#include "libavutil/channel_layout.h"
#include "libavutil/mathematics.h"
#include "libavutil/opt.h"
#include "libavformat/avformat.h"
#include "libavresample/avresample.h"
#include "libswscale/swscale.h"

const int ERtmpFrameCount = 10;

struct FMyFrame {
    FColor *Pixels;
};

typedef struct LibAVOutputStream {
    AVStream *st;

    /* pts of the next frame that will be generated */
    int64_t next_pts;

    AVFrame *frame;
    AVFrame *tmp_frame;

    struct SwsContext *sws_ctx;
    AVAudioResampleContext *avr;
} LibAVOutputStream;


class FMyRtmpConnection {
public:
    FMyRtmpConnection( const FLiveStreamInfo &Info );
    virtual ~FMyRtmpConnection();

    bool Start( const FGameBroadcastConfig &Config );
    bool PushFrame( const FMyFrame &frame );
    bool Shutdown();

    bool IsActive() const { return Active; }

    FLiveStreamInfo &GetLiveStreamInfo() const { return StreamInfo; }

private:
    bool OpenVideo();
    void AddVideoStream();
    AVFrame *ConvertFrame( const FMyFrame &Frame );
    void ConvertFrameInternal( const FMyFrame &Frame );

    bool Active;

    FGameBroadcastConfig BroadcastConfig;
    FLiveStreamInfo StreamInfo;

    LibAVOutputStream AvStream;
    AVOutputFormat *AvOutFormat;
    AVFormatContext *AvFormatContext;
};

class FRtmpLiveStreamingService: public ILiveStreamingService {
public:
    FRtmpLiveStreamingService();

    FOnStatusChanged OnStatusChanged() { return StatusChangedDelegate; }

    void StartBroadcasting( const FBroadcastConfig& Config );
	void StopBroadcasting();
	bool IsBroadcasting() const;
    bool IsReadyForVideoFrames() const;

	void MakeValidVideoBufferResolution( int& VideoBufferWidth, int& VideoBufferHeight ) const;
	void QueryBroadcastConfig( FBroadcastConfig& OutBroadcastConfig ) const;

    void PushVideoFrame( const FColor* VideoFrameBuffer );

    void StartWebCam( const FWebCamConfig& Config ) {}
    void StopWebCam() {}
	bool IsWebCamEnabled() const { return false; }

	UTexture2D* GetWebCamTexture( bool& bIsImageFlippedHorizontally, 
            bool& bIsImageFlippedVertically ) { return nullptr; }

	FOnChatMessage& OnChatMessage() { return ChatMessageDelegate; }
	void ConnectToChat() {}
	void DisconnectFromChat() {}
	bool IsChatEnabled() const { return false; }
	void SendChatMessage( const FString& ChatMessage ) {}

	void QueryLiveStreams( const FString& GameName, 
            FQueryLiveStreamsCallback CompletionCallback );

    bool HasPendingVideoFrames() const;

    friend class FMyFrameEncodeThreadRunnable;

private:
    void StartBroadcastThread();
    void StopBroadcastThread();
    void FeedNextFrame();

    FOnStatusChanged StatusChangedDelegate;
    FOnChatMessage   ChatMessageDelegate;

    FBroadcastConfig ActiveConfig;

    FCriticalSection FramesLock;
    FMyFrame Frames[ ERtmpFrameCount ];
    int32 FrameStartIndex;
    int32 FrameEndIndex;

    TArray<TSharedPtr<FMyRtmpConnection>> Connections;

    TArray<FLiveStreamInfo> DefaultStreams;

    int32 BroadcastActive;

    FRunnableThread *BroadcastThread;
    TUniquePtr<FRunnable> BroadcastThreadRunnable;
};
