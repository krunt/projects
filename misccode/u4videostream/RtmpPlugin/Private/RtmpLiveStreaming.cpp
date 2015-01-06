
#include "RtmpPluginPrivatePCH.h"
#include "RtmpLiveStreaming.h"
#include "Runtime/Core/Public/Features/IModularFeatures.h"

#include <stdio.h>

FMyRtmpConnection::FMyRtmpConnection(const FLiveStreamInfo &Info)
	: StreamInfo(Info), Active(false)
{

}

FMyRtmpConnection::~FMyRtmpConnection() {
	Shutdown();
}

bool FMyRtmpConnection::Start(const FBroadcastConfig &Config) {
	BroadcastConfig = Config;

	av_register_all();

	memset(&AvStream, 0, sizeof(AvStream));

	AvOutFormat = av_guess_format("flv", NULL, NULL);

	if (!AvOutFormat) {
		UE_LOG(LogTemp, Warning, TEXT("Can't find flv format"));
		return false;
	}

	AvFormatContext = avformat_alloc_context();
	if (!AvFormatContext) {
		UE_LOG(LogTemp, Warning, TEXT("Can't allocate format context"));
		return false;
	}

	/*
	FString Filename = StreamInfo.URL;
	Filename += TEXT("/");
	Filename += StreamInfo.StreamName;
	*/


	const char *filename = "rtmp://192.168.0.105:1935/myapp/mystream";

	AvFormatContext->oformat = AvOutFormat;
	sprintf_s(AvFormatContext->filename, 
		sizeof(AvFormatContext->filename),
		"%s", filename);

	UE_LOG(LogTemp, Log, TEXT("Trying to open "));
		//(void*)AvFormatContext->filename);

	if (AvOutFormat->video_codec == AV_CODEC_ID_NONE) {
		UE_LOG(LogTemp, Warning, TEXT("Video codec is none"));
		return false;
	}

	if (!AddVideoStream()) {
		return false;
	}

	if (!OpenVideo()) {
		return false;
	}

	av_dump_format(AvFormatContext, 0, AvFormatContext->filename, 1);

	if (!(AvOutFormat->flags & AVFMT_NOFILE)) {
		if (avio_open(&AvFormatContext->pb, filename, AVIO_FLAG_WRITE) < 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("Could not open"));
				//(void*)AvFormatContext->filename);
			return false;
		}
	}

	avformat_write_header(AvFormatContext, NULL);

	Active = true;

	UE_LOG(LogTemp, Log, TEXT("Started rtmp connection..."));
	return true;
}

bool FMyRtmpConnection::AddVideoStream() {
	LibAVOutputStream *ost = &AvStream;
	AVCodecContext *c;
	AVCodec *codec;
	AVFormatContext *oc = AvFormatContext;

	/* find the video encoder */
	codec = avcodec_find_encoder(AvOutFormat->video_codec);
	if (!codec) {
		UE_LOG(LogTemp, Warning, TEXT("Can't find video encoder"));
		return false;
	}

	ost->st = avformat_new_stream(oc, codec);
	if (!ost->st) {
		UE_LOG(LogTemp, Warning, TEXT("Can't create video stream"));
		return false;
	}

	UE_LOG(LogTemp, Warning, TEXT("%d/%d/%d"),
		BroadcastConfig.VideoBufferWidth,
		BroadcastConfig.VideoBufferHeight,
		BroadcastConfig.FramesPerSecond);

	c = ost->st->codec;

	c->qmin = 4;
	c->qmax = 2;

	/* Put sample parameters. */
	c->bit_rate = 4000000;
	/* Resolution must be a multiple of two. */
	c->width = BroadcastConfig.VideoBufferWidth;
	c->height = BroadcastConfig.VideoBufferHeight;
	/* timebase: This is the fundamental unit of time (in seconds) in terms
	* of which frame timestamps are represented. For fixed-fps content,
	* timebase should be 1/framerate and timestamp increments should be
	* identical to 1. */
	ost->st->time_base.num = 1;
	ost->st->time_base.den = BroadcastConfig.FramesPerSecond;

	c->time_base = ost->st->time_base;

	c->gop_size = 12; /* emit one intra frame every twelve frames at most */
	c->pix_fmt = AV_PIX_FMT_YUV420P;
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

AVFrame *FMyRtmpConnection::AllocPicture(enum AVPixelFormat pix_fmt, 
	int width, int height)
{
	AVFrame *picture;
	int ret;

	picture = av_frame_alloc();
	if (!picture)
		return NULL;

	picture->format = pix_fmt;
	picture->width = width;
	picture->height = height;

	/* allocate the buffers for the frame data */
	ret = av_frame_get_buffer(picture, 32);
	if (ret < 0) {
		return NULL;
	}

	return picture;
}

bool FMyRtmpConnection::OpenVideo() {
	LibAVOutputStream *ost = &AvStream;
	AVCodecContext *c;

	c = AvStream.st->codec;

	/* open the codec */
	if (avcodec_open2(c, NULL, NULL) < 0) {
		UE_LOG(LogTemp, Warning, TEXT("Can't open video codec"));
		return false;
	}

	/* Allocate the encoded raw picture. */
	AvStream.frame = AllocPicture(c->pix_fmt, c->width, c->height);
	if (!ost->frame) {
		UE_LOG(LogTemp, Warning, TEXT("Unable allocate frame"));
		return false;
	}

	AvStream.tmp_frame = AllocPicture(AV_PIX_FMT_RGB24,
		BroadcastConfig.VideoBufferWidth, BroadcastConfig.VideoBufferHeight);

	if (!AvStream.tmp_frame) {
		UE_LOG(LogTemp, Warning, TEXT("Unable allocate temp_frame"));
		return false;
	}

	return true;
}

void FMyRtmpConnection::ConvertFrameInternal(const FMyFrame &Frame) {
	int ret, x, y;
	int width, height;
	AVFrame *Pict = AvStream.tmp_frame;

	ret = av_frame_make_writable(Pict);
	if (ret < 0)
		return;
	
	width = BroadcastConfig.VideoBufferWidth;
	height = BroadcastConfig.VideoBufferHeight;

	for (y = 0; y < height; ++y) {
		for (x = 0; x < width; ++x) {

			FColor &color = Frame.Pixels[y * width + x];

			uint8 R, G, B, A;

			R = G = B = A = 0;

			switch (BroadcastConfig.PixelFormat) {
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


			*(Pict->data[0] + y * Pict->linesize[0] + 3 * x + 0) = R;
			*(Pict->data[0] + y * Pict->linesize[0] + 3 * x + 1) = G;
			*(Pict->data[0] + y * Pict->linesize[0] + 3 * x + 2) = B;
		}
	}
}

AVFrame *FMyRtmpConnection::ConvertFrame(const FMyFrame &Frame) {
	LibAVOutputStream *ost = &AvStream;
	AVCodecContext *c = ost->st->codec;

	if (!ost->sws_ctx) {
		ost->sws_ctx = sws_getContext(BroadcastConfig.VideoBufferWidth,
			BroadcastConfig.VideoBufferHeight,
			AV_PIX_FMT_RGB24,
			c->width, c->height,
			c->pix_fmt,
			SWS_BICUBIC, NULL, NULL, NULL);
		if (!ost->sws_ctx) {
			UE_LOG(LogTemp, Warning, TEXT("Unable allocate sws_ctx, deactivating"));
			return nullptr;
		}
	}

	ConvertFrameInternal( Frame );


	sws_scale(ost->sws_ctx, ost->tmp_frame->data, ost->tmp_frame->linesize,
		0, BroadcastConfig.VideoBufferHeight, ost->frame->data, ost->frame->linesize);

	AVFrame *res = AvStream.frame;
		
	res->pts = ost->next_pts++;

	return res;
}

bool FMyRtmpConnection::PushFrame(const FMyFrame &Frame) {
	int ret;
	int GotPacket = 0;
	AVPacket Pkt = { 0 };
	AVFormatContext *oc = AvFormatContext;
	LibAVOutputStream *ost = &AvStream;
	AVCodecContext *c = ost->st->codec;

	AVFrame *LibAvFrame = ConvertFrame(Frame);

	if (!IsActive()) {
		return false;
	}

	if (!LibAvFrame) {
		Active = false;
		return false;
	}

	av_init_packet(&Pkt);

	/* encode the image */
	ret = avcodec_encode_video2(c, &Pkt, LibAvFrame, &GotPacket);
	if (ret < 0) {
		UE_LOG(LogTemp, Warning, TEXT("encode video error, deactivating"));
		Active = false;
		return false;
	}

	if (GotPacket) {
		av_packet_rescale_ts(&Pkt, c->time_base, ost->st->time_base);
		Pkt.stream_index = ost->st->index;
		ret = av_write_frame(oc, &Pkt);
	}

	if (ret != 0) {
		UE_LOG(LogTemp, Warning, TEXT("write frame error, deactivating"));
		Active = false;
		return false;
	}

	return true;
}

void FMyRtmpConnection::CloseStream() {
	avcodec_close(AvStream.st->codec);
	av_frame_free(&AvStream.frame);
	av_frame_free(&AvStream.tmp_frame);
	sws_freeContext(AvStream.sws_ctx);
}

bool FMyRtmpConnection::Shutdown() {
	av_write_trailer(AvFormatContext);
	CloseStream();
	avformat_free_context(AvFormatContext);

	return true;
}

void FRtmpLiveStreamingService::Clear() {
	FrameStartIndex = FrameEndIndex = 0;

	for (int32 i = 0; i < ERtmpFrameCount; ++i) {
		if (Frames[i].Pixels) {
			delete[] Frames[i].Pixels;
			Frames[i].Pixels = nullptr;
		}
	}

	Connections.Empty();
}

FRtmpLiveStreamingService::FRtmpLiveStreamingService() 
	: PlayRandomMovieCommand( TEXT("RtmpPlay.Start"),
	TEXT("Starts generating faked rtmp\n")
	TEXT("\n")
	TEXT("Optional Params:\n")
	TEXT("Duration=<int32> - Seconds of play"),
	FConsoleCommandWithArgsDelegate::CreateStatic(
	[]( const TArray< FString > &Args ) 
	{
		int32 Duration = 10;
		for (FString Arg : Args)
		{
			FParse::Value(*Arg, TEXT("Duration="), Duration);
		}

		FRtmpLiveStreamingService& Service =
			IModularFeatures::Get().GetModularFeature<FRtmpLiveStreamingService>(
				"LiveStreaming");
		if (Service.IsBroadcasting()) {
			FBroadcastConfig Config;
			Service.QueryBroadcastConfig(Config);
			int32 NumFrames = Config.FramesPerSecond * Duration;
			FColor *c = new FColor[Config.VideoBufferWidth
				*Config.VideoBufferHeight];
			
			for (int i = 0; i < NumFrames; ++i) {
				for (int j = 0; j < Config.VideoBufferWidth
					*Config.VideoBufferHeight; ++j)
				{
					c[j].R = i & 255;
					c[j].G = (j / Config.VideoBufferWidth) & 255;
					c[j].B = (j % Config.VideoBufferWidth) & 255;
					c[j].A = 0x80;
				}
	
				while (!Service.IsReadyForVideoFrames()) {
					if (!Service.IsBroadcasting())
					{
						return;
					}
					FPlatformProcess::Sleep(0.001);
				}
				if (Service.IsReadyForVideoFrames()) 
				{
					Service.PushVideoFrame(c);
				}
			}
			delete[] c;
		}
	}))
{
	BroadcastActive = 0;

	bFramesIsFull = false;

	for (int i = 0; i < ERtmpFrameCount; ++i) {
		Frames[i].Pixels = nullptr;
	}

	FLiveStreamInfo nf;
	
	nf.StreamName = "mystream";
	nf.URL = "rtmp://192.168.0.105/myapp";

	DefaultStreams.Push( nf );
}

void FRtmpLiveStreamingService::StartBroadcasting(const FBroadcastConfig& Config) {
	if (BroadcastActive) {
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("StartBroadcasting()"));

	/*
	check(FMath::IsPowerOfTwo(Config.VideoBufferWidth));
	check(FMath::IsPowerOfTwo(Config.VideoBufferHeight));
	*/

	ActiveConfig = Config;

	Clear();

	for (auto s : DefaultStreams) {
		TSharedPtr<FMyRtmpConnection> Conn(new FMyRtmpConnection(s));
		if (Conn->Start(Config)) {
			Connections.Push(Conn);
		}
	}

	if (Connections.Num() > 0) {
		FPlatformAtomics::InterlockedIncrement(&BroadcastActive);
		StartBroadcastThread();

		UE_LOG(LogTemp, Display, TEXT("Started broadcasting with %d connections"),
			Connections.Num());
	}
}

void FRtmpLiveStreamingService::StopBroadcasting() {
	UE_LOG(LogTemp, Log, TEXT("StopBroadcasting()"));
	FPlatformAtomics::InterlockedDecrement(&BroadcastActive);
}

bool FRtmpLiveStreamingService::IsBroadcasting() const {
	return BroadcastActive != 0;
}

bool FRtmpLiveStreamingService::IsReadyForVideoFrames() const {
	bool IsReady = false;

	FramesLock.Lock();
	IsReady = !bFramesIsFull;
	FramesLock.Unlock();

	return IsReady;
}

bool FRtmpLiveStreamingService::HasPendingVideoFrames() const {
	bool HasFrames = false;

	FramesLock.Lock();
	HasFrames = FrameStartIndex != FrameEndIndex || bFramesIsFull;
	FramesLock.Unlock();

	return HasFrames;
}

void FRtmpLiveStreamingService::PushVideoFrame(const FColor* VideoFrameBuffer) {
	if (!IsReadyForVideoFrames()) {
		UE_LOG(LogTemp, Log, TEXT("PushVideoFrame() is not ready yet"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("PushVideoFrame()"));

	int32 NextFrameIndex = 0;
	{
		FramesLock.Lock();
		NextFrameIndex = FrameEndIndex;
		FramesLock.Unlock();
	}

	FMyFrame &frame = Frames[NextFrameIndex];

	int32 numPixels = ActiveConfig.VideoBufferWidth * ActiveConfig.VideoBufferHeight;
	if (!frame.Pixels) {

		frame.Pixels = new FColor[numPixels];
	}

	memcpy(frame.Pixels, VideoFrameBuffer, numPixels * sizeof(FColor));

	{
		FramesLock.Lock();
		FrameEndIndex++;
		FrameEndIndex %= ERtmpFrameCount;
		bFramesIsFull = FrameStartIndex == FrameEndIndex;
		FramesLock.Unlock();
	}
}

class FMyFrameEncodeThreadRunnable : public FRunnable {
public:
	FMyFrameEncodeThreadRunnable(FRtmpLiveStreamingService &Service) 
		: RtmpService( Service )
	{}

	bool Init() { return true; }
	void Exit() {}
	void Stop() {}
	uint32 Run();

private:
	FRtmpLiveStreamingService &RtmpService;
};

uint32 FMyFrameEncodeThreadRunnable::Run() {
	
	UE_LOG(LogTemp, Log, TEXT("Rtmp thread started with `%d'"),
		(int)RtmpService.IsBroadcasting());

	int32 LogCount = 0;

	while (RtmpService.IsBroadcasting()) {

		if (!(LogCount % 1000)) {
			UE_LOG(LogTemp, Log, TEXT("Rtmp thread is working %d/%d"),
				(int)RtmpService.HasPendingVideoFrames(),
				(int)RtmpService.IsReadyForVideoFrames());
		}

		if (!RtmpService.HasPendingVideoFrames()) {
			FPlatformProcess::Sleep(0.01);
			LogCount++;
			continue;
		}

		RtmpService.FeedNextFrame();
		LogCount++;
	}

	UE_LOG(LogTemp, Log, TEXT("Rtmp thread stopped with `%d'"),
		(int)RtmpService.IsBroadcasting());

	RtmpService.Clear();

	return 0;
}

void FRtmpLiveStreamingService::StartBroadcastThread() {
	/*
	BroadcastThread = nullptr;
	BroadcastThreadRunnable = TUniquePtr<FRunnable>(
		new FMyFrameEncodeThreadRunnable(*this) );
	BroadcastThread = TUniquePtr<FRunnableThread>(FRunnableThread::Create(
		BroadcastThreadRunnable.Get(), TEXT("RtmpLiveStreamHelper"), 
		16 * 1024, TPri_Normal, FPlatformAffinity::GetPoolThreadMask()));
		*/
	
	BroadcastThreadRunnable = new FMyFrameEncodeThreadRunnable(*this);
	BroadcastThread = FRunnableThread::Create(
		BroadcastThreadRunnable, TEXT("RtmpLiveStreamHelper"),
		16 * 1024, TPri_Normal, FPlatformAffinity::GetPoolThreadMask());
		
}

void FRtmpLiveStreamingService::StopBroadcastThread() {
}

void FRtmpLiveStreamingService::FeedNextFrame() {
	check(IsBroadcasting() && HasPendingVideoFrames());

	UE_LOG(LogTemp, Log, TEXT("FeedNextFrame()"));

	int32 NextFrameIndex = 0;

	FramesLock.Lock();
	NextFrameIndex = FrameStartIndex;
	FramesLock.Unlock();

	FMyFrame &Frame = Frames[NextFrameIndex];

	check(Frame.Pixels);

	for (auto Conn : Connections) {
		Conn->PushFrame(Frame);
	}

	FramesLock.Lock();
	FrameStartIndex++;
	FrameStartIndex %= ERtmpFrameCount;
	bFramesIsFull = false;
	FramesLock.Unlock();
}

static int GetNextPowerOfTwo(int v) {
	v = v | v >> 16;
	v = v | v >> 8;
	v = v | v >> 4;
	v = v | v >> 2;
	v = v | v >> 1;
	return v + 1;
}

void FRtmpLiveStreamingService::MakeValidVideoBufferResolution(int& VideoBufferWidth, int& VideoBufferHeight) const
{
	UE_LOG(LogTemp, Log, TEXT("MakeValidVideoBufferResolution(%d/%d)"), 
		VideoBufferWidth, VideoBufferHeight);

	//VideoBufferWidth = 1024 / 4;
	//VideoBufferHeight = 768 / 4;

	VideoBufferWidth = 1280;
	VideoBufferHeight = 720;

	/*
	if (!FMath::IsPowerOfTwo(VideoBufferWidth)) {
		VideoBufferWidth = GetNextPowerOfTwo(VideoBufferWidth);
	}

	if (!FMath::IsPowerOfTwo(VideoBufferHeight)) {
		VideoBufferHeight = GetNextPowerOfTwo(VideoBufferHeight);
	}
	*/
}

void FRtmpLiveStreamingService::QueryBroadcastConfig(FBroadcastConfig& OutBroadcastConfig) const {
	check(IsBroadcasting());
	OutBroadcastConfig = ActiveConfig;
}

void FRtmpLiveStreamingService::QueryLiveStreams(const FString& GameName,
	FQueryLiveStreamsCallback CompletionCallback)
{
	TArray<FLiveStreamInfo> Infos;
	for (auto Conn : Connections) {
		Infos.Push(Conn->GetLiveStreamInfo());
	}

	CompletionCallback.ExecuteIfBound(Infos, true);
}
