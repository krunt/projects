
#pragma once

extern "C" {
#include "libavutil/channel_layout.h"
#include "libavutil/mathematics.h"
#include "libavutil/opt.h"
#include "libavformat/avformat.h"
#include "libavresample/avresample.h"
#include "libswscale/swscale.h"
}

#include "Runtime/Engine/Public/Features/ILiveStreamingService.h"
#include "Runtime/Core/Public/Templates/UniquePtr.h"

const int ERtmpFrameCount = 100;

struct FMyFrame {
	FColor *Pixels;
};

typedef struct LibAVOutputStream {
	AVStream *st;

	int64_t next_pts;

	AVFrame *frame;
	AVFrame *tmp_frame;

	struct SwsContext *sws_ctx;
	AVAudioResampleContext *avr;
} LibAVOutputStream;


class FMyRtmpConnection {
public:
	FMyRtmpConnection(const FLiveStreamInfo &Info);
	virtual ~FMyRtmpConnection();

	bool Start(const FBroadcastConfig &Config);
	bool PushFrame(const FMyFrame &frame);
	bool Shutdown();

	bool IsActive() const { return Active; }

	const FLiveStreamInfo &GetLiveStreamInfo() const { return StreamInfo; }

private:
	bool OpenVideo();
	bool AddVideoStream();
	AVFrame *ConvertFrame(const FMyFrame &Frame);
	void ConvertFrameInternal(const FMyFrame &Frame);
	AVFrame *FMyRtmpConnection::AllocPicture(enum AVPixelFormat pix_fmt,
		int width, int height);
	void CloseStream();

	bool Active;

	FBroadcastConfig BroadcastConfig;
	FLiveStreamInfo StreamInfo;

	LibAVOutputStream AvStream;
	AVOutputFormat *AvOutFormat;
	AVFormatContext *AvFormatContext;
};

class FRtmpLiveStreamingService : public ILiveStreamingService {
public:
	FRtmpLiveStreamingService();
	virtual ~FRtmpLiveStreamingService() { Clear();  }

	FOnStatusChanged &OnStatusChanged() { return StatusChangedDelegate; }

	virtual void StartBroadcasting(const FBroadcastConfig& Config) override;
	virtual void StopBroadcasting() override;
	virtual bool IsBroadcasting() const  override;
	virtual bool IsReadyForVideoFrames() const  override;

	virtual void MakeValidVideoBufferResolution(int& VideoBufferWidth, int& VideoBufferHeight) const  override;
	virtual void QueryBroadcastConfig(FBroadcastConfig& OutBroadcastConfig) const  override;

	virtual void PushVideoFrame(const FColor* VideoFrameBuffer)  override;

	virtual void StartWebCam(const FWebCamConfig& Config)  override {}
	virtual void StopWebCam()  override {}
	virtual bool IsWebCamEnabled() const  override { return false; }

	virtual UTexture2D* GetWebCamTexture(bool& bIsImageFlippedHorizontally,
		bool& bIsImageFlippedVertically)  override {
		return nullptr;
	}

	virtual FOnChatMessage& OnChatMessage()  override { return ChatMessageDelegate; }
	virtual void ConnectToChat()  override {}
	virtual void DisconnectFromChat()  override {}
	virtual bool IsChatEnabled() const  override { return false; }
	virtual void SendChatMessage(const FString& ChatMessage)  override {}

	virtual void QueryLiveStreams(const FString& GameName,
		FQueryLiveStreamsCallback CompletionCallback)  override;

	bool HasPendingVideoFrames() const;

	friend class FMyFrameEncodeThreadRunnable;

private:
	void StartBroadcastThread();
	void StopBroadcastThread();
	void FeedNextFrame();
	void Clear();

	FOnStatusChanged StatusChangedDelegate;
	FOnChatMessage   ChatMessageDelegate;

	FBroadcastConfig ActiveConfig;

	mutable FCriticalSection FramesLock;
	FMyFrame Frames[ERtmpFrameCount];
	int32 FrameStartIndex;
	int32 FrameEndIndex;
	bool  bFramesIsFull;

	TArray<TSharedPtr<FMyRtmpConnection>> Connections;

	TArray<FLiveStreamInfo> DefaultStreams;

	int32 BroadcastActive;

	/*
	TUniquePtr<FRunnableThread> BroadcastThread;
	TUniquePtr<FRunnable> BroadcastThreadRunnable;
	*/
	FRunnableThread *BroadcastThread;
	FRunnable *BroadcastThreadRunnable;

	FAutoConsoleCommand PlayRandomMovieCommand;
};
