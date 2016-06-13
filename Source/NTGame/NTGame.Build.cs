// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class NTGame : ModuleRules
{
	public NTGame(TargetInfo Target)
	{
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "OnlineSubSystem", "OnlineSubsystemUtils" });

		PrivateDependencyModuleNames.AddRange(new string[] {  });

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

//         PrivateDependencyModuleNames.Add("OnlineSubsystem");
//         if ((Target.Platform == UnrealTargetPlatform.Win32) || (Target.Platform == UnrealTargetPlatform.Win64))
//         {
//             if (UEBuildConfiguration.bCompileSteamOSS == true)
//             {
//                 DynamicallyLoadedModuleNames.Add("OnlineSubsystemSteam");
//             }
//         }
	}
}
