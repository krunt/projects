// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "RtmpPluginPrivatePCH.h"
#include "RtmpLiveStreaming.h"
#include "Runtime/Core/Public/Features/IModularFeatures.h"

class FRtmpPlugin : public IRtmpPlugin
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

IMPLEMENT_MODULE( FRtmpPlugin, RtmpPlugin )



void FRtmpPlugin::StartupModule()
{
	// This code will execute after your module is loaded into memory (but after global variables are initialized, of course.)
	if ( !IModularFeatures::Get().IsModularFeatureAvailable("LiveStreaming") ) 
	{
		IModularFeatures::Get().RegisterModularFeature("LiveStreaming",
			new FRtmpLiveStreamingService());
		UE_LOG(LogTemp, Display, TEXT("Registered rtmp live streamer feature"));
	}
}


void FRtmpPlugin::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}



