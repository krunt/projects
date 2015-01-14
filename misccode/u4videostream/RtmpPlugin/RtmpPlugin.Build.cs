// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class RtmpPlugin : ModuleRules
	{
		public RtmpPlugin(TargetInfo Target)
		{
			if ( Target.Platform == UnrealTargetPlatform.Win64) {
				AddThirdPartyPrivateStaticDependencies(Target, "libav");
			}
			
				PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					// ... add other public dependencies that you statically link with here ...
				}
				);
			
		}
	}
}