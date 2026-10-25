// Copyright Epic Games, Inc. All Rights Reserved.

// Route acceptance driver: in a -game session, possesses the tagged van and
// follows the "Main" road polyline from Saved/WorldGeneration/World.json with a
// simple lookahead autopilot, then writes Saved/WorldGeneration/VanRouteMain.json
// and exits. Console command: QRW.RouteDrive

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "Engine/GameEngine.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "HAL/PlatformProcess.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "VehicleTemplate/Core/RoadToWisdomPawn.h"
#include "RoadToWisdom.h"

namespace QRWRouteDrive
{
	constexpr float WaypointRadiusCm = 2200.0f;   // advance to the next point inside 22 m
	constexpr float StallSpeedCm = 40.0f;         // below this under throttle...
	constexpr float StallTimeoutS = 8.0f;         // ...for this long = stuck
	constexpr float MaxRunSeconds = 480.0f;
	constexpr float OffroadLimitCm = 750.0f;      // 4.5 m half width + shoulder

	struct FRouteState
	{
		TArray<FVector> Points;
		float RouteLengthCm = 0.0f;
		int32 Waypoint = 1;
		double StartSeconds = 0.0;
		double StallSince = -1.0;
		int32 OffroadSamples = 0;
		int32 Samples = 0;
		double SpeedSum = 0.0;
		float SpeedMax = 0.0f;
		bool bFinished = false;
	};

	static FRouteState State;
	static FTSTicker::FDelegateHandle TickHandle;

	static float RouteLengthM()
	{
		return State.RouteLengthCm / 100.0f;
	}

	static void Finish(UWorld* World, const FString& Reason)
	{
		if (State.bFinished)
		{
			return;
		}
		State.bFinished = true;

		const double Elapsed = FPlatformTime::Seconds() - State.StartSeconds;
		const float Progress = static_cast<float>(State.Waypoint) / FMath::Max(1, State.Points.Num() - 1);
		float ArcCm = 0.0f;
		for (int32 i = 0; i < State.Waypoint && i + 1 < State.Points.Num(); ++i)
		{
			ArcCm += FVector::Dist2D(State.Points[i], State.Points[i + 1]);
		}
		const float AvgKmh = State.Samples > 0 ? (State.SpeedSum / State.Samples) * 0.036f : 0.0f;
		const float MaxKmh = State.SpeedMax * 0.036f;

		TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("reason"), Reason);
		Root->SetNumberField(TEXT("route_points"), State.Points.Num());
		Root->SetNumberField(TEXT("route_length_m"), RouteLengthM());
		Root->SetNumberField(TEXT("reached_waypoint"), State.Waypoint);
		Root->SetNumberField(TEXT("progress"), Progress);
		Root->SetNumberField(TEXT("travelled_m"), ArcCm / 100.0);
		Root->SetNumberField(TEXT("elapsed_s"), Elapsed);
		Root->SetNumberField(TEXT("avg_kmh"), AvgKmh);
		Root->SetNumberField(TEXT("max_kmh"), MaxKmh);
		Root->SetNumberField(TEXT("offroad_samples_over_limit"), State.OffroadSamples);
		Root->SetNumberField(TEXT("samples"), State.Samples);
		Root->SetBoolField(TEXT("passed"),
			Reason == TEXT("completed") && Progress >= 0.95f && AvgKmh >= 12.0f &&
			State.OffroadSamples <= State.Samples * 0.05f);

		FString Output;
		const auto Writer = TJsonWriterFactory<>::Create(&Output);
		FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
		const FString Path = FPaths::ProjectSavedDir() / TEXT("WorldGeneration/VanRouteMain.json");
		FFileHelper::SaveStringToFile(Output, *Path);

		UE_LOG(LogRoadToWisdom, Warning, TEXT("[ROUTE-RESULT] %s"), *Output);

		if (TickHandle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
			TickHandle.Reset();
		}
		FPlatformMisc::RequestExit(false);
	}

	static bool TickTicker(float)
	{
		if (!GEngine || State.bFinished || State.Points.Num() < 2)
		{
			return true;
		}
		UWorld* PlayWorld = GEngine->GetCurrentPlayWorld();
		if (!PlayWorld)
		{
			return true;
		}

		const double Now = FPlatformTime::Seconds();
		if (Now - State.StartSeconds > MaxRunSeconds)
		{
			Finish(PlayWorld, TEXT("timeout"));
			return false;
		}

		APlayerController* PC = PlayWorld->GetFirstPlayerController();
		APawn* Van = PC ? PC->GetPawn() : nullptr;
		UChaosWheeledVehicleMovementComponent* Movement = Van ? Van->FindComponentByClass<UChaosWheeledVehicleMovementComponent>() : nullptr;
		if (!Van || !Movement)
		{
			return true;
		}

		const FVector Pos = Van->GetActorLocation();

		while (State.Waypoint < State.Points.Num() - 1 &&
			FVector::Dist2D(Pos, State.Points[State.Waypoint]) < WaypointRadiusCm)
		{
			++State.Waypoint;
		}

		const FVector Look = State.Points[FMath::Min(State.Waypoint + 3, State.Points.Num() - 1)];
		FVector ToTarget = Look - Pos;
		ToTarget.Z = 0.0;
		ToTarget.Normalize();
		FVector Forward = Van->GetActorForwardVector();
		Forward.Z = 0.0;
		Forward.Normalize();
		const float Cross = Forward.X * ToTarget.Y - Forward.Y * ToTarget.X;
		const float Dot = FMath::Max(0.2f, static_cast<float>(Forward.X * ToTarget.X + Forward.Y * ToTarget.Y));
		const float Steer = FMath::Clamp(2.2f * FMath::Sign(Cross) * FMath::Min(1.0f, FMath::Abs(Cross) / Dot * 1.4f), -1.0f, 1.0f);
		Movement->SetSteeringInput(Steer);

		const FVector Ahead = State.Points[FMath::Min(State.Waypoint + 14, State.Points.Num() - 1)];
		const float Curve = static_cast<float>(FVector::Dist2D(Ahead, Look));
		const float Speed = Movement->GetForwardSpeed();
		const float Want = Curve > 16000.0f ? 1400.0f : 900.0f;
		if (Speed < Want)
		{
			Movement->SetThrottleInput(Curve > 16000.0f ? 0.85f : 0.6f);
			Movement->SetBrakeInput(0.0f);
		}
		else
		{
			Movement->SetThrottleInput(0.0f);
			Movement->SetBrakeInput(0.4f);
		}
		Movement->SetHandbrakeInput(false);

		double OffCm = TNumericLimits<double>::Max();
		for (int32 i = FMath::Max(0, State.Waypoint - 6); i < FMath::Min(State.Points.Num() - 1, State.Waypoint + 12); ++i)
		{
			const FVector2D A(State.Points[i].X, State.Points[i].Y);
			const FVector2D B(State.Points[i + 1].X, State.Points[i + 1].Y);
			const FVector2D D = B - A;
			const float DD = D.SizeSquared();
			const FVector2D P(Pos.X, Pos.Y);
			const float T = DD > 0.0f ? FMath::Clamp(FVector2D::DotProduct(P - A, D) / DD, 0.0f, 1.0f) : 0.0f;
			OffCm = FMath::Min(OffCm, static_cast<double>(FVector2D::Distance(P, A + T * D)));
		}
		if (OffCm > OffroadLimitCm)
		{
			++State.OffroadSamples;
		}

		++State.Samples;
		State.SpeedSum += FMath::Abs(Speed);
		State.SpeedMax = FMath::Max(State.SpeedMax, FMath::Abs(Speed));

		if (FMath::Abs(Speed) < StallSpeedCm)
		{
			if (State.StallSince < 0.0)
			{
				State.StallSince = Now;
			}
			else if (Now - State.StallSince > StallTimeoutS)
			{
				Finish(PlayWorld, TEXT("stalled"));
				return false;
			}
		}
		else
		{
			State.StallSince = -1.0;
		}

		if (State.Waypoint >= State.Points.Num() - 2)
		{
			Finish(PlayWorld, TEXT("completed"));
			return false;
		}
		return true;
	}

	static bool LoadRoute()
	{
		const FString Path = FPaths::ProjectSavedDir() / TEXT("WorldGeneration/World.json");
		FString Json;
		if (!FFileHelper::LoadFileToString(Json, *Path))
		{
			UE_LOG(LogRoadToWisdom, Error, TEXT("[ROUTE] World.json not found at %s"), *Path);
			return false;
		}
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			return false;
		}
		const TArray<TSharedPtr<FJsonValue>>* Roads = nullptr;
		if (!Root->TryGetArrayField(TEXT("roads"), Roads))
		{
			return false;
		}
		for (const TSharedPtr<FJsonValue>& RoadValue : *Roads)
		{
			const TSharedPtr<FJsonObject> Road = RoadValue->AsObject();
			FString Name;
			if (Road.IsValid() && Road->TryGetStringField(TEXT("name"), Name) && Name == TEXT("Main"))
			{
				const TArray<TSharedPtr<FJsonValue>>* Points = nullptr;
				if (Road->TryGetArrayField(TEXT("points"), Points))
				{
					for (const TSharedPtr<FJsonValue>& PointValue : *Points)
					{
						const TArray<TSharedPtr<FJsonValue>>* C = nullptr;
						if (PointValue->TryGetArray(C) && C->Num() >= 3)
						{
							State.Points.Emplace((*C)[0]->AsNumber() * 100.0, (*C)[1]->AsNumber() * 100.0, (*C)[2]->AsNumber() * 100.0);
						}
					}
				}
				break;
			}
		}
		State.RouteLengthCm = 0.0f;
		for (int32 i = 0; i + 1 < State.Points.Num(); ++i)
		{
			State.RouteLengthCm += FVector::Dist2D(State.Points[i], State.Points[i + 1]);
		}
		return State.Points.Num() > 2;
	}

	static void StartRouteDrive(const TArray<FString>& Args)
	{
		UGameEngine* Engine = GEngine ? Cast<UGameEngine>(GEngine) : nullptr;
		UWorld* World = Engine ? Engine->GetCurrentPlayWorld() : nullptr;
		if (!World)
		{
			UE_LOG(LogRoadToWisdom, Error, TEXT("[ROUTE] no play world"));
			return;
		}
		if (!LoadRoute())
		{
			UE_LOG(LogRoadToWisdom, Error, TEXT("[ROUTE] route load failed"));
			return;
		}

		APlayerController* PC = World->GetFirstPlayerController();
		if (!PC)
		{
			UE_LOG(LogRoadToWisdom, Error, TEXT("[ROUTE] no player controller"));
			return;
		}

		// Prefer the tagged placed van; otherwise the possessed default pawn drives.
		ARoadToWisdomPawn* Tagged = nullptr;
		for (TActorIterator<ARoadToWisdomPawn> It(World); It; ++It)
		{
			if (It->ActorHasTag(TEXT("QuietRoad.Van")))
			{
				Tagged = *It;
				break;
			}
		}
		if (Tagged)
		{
			PC->Possess(Tagged);
		}
		if (!PC->GetPawn())
		{
			UE_LOG(LogRoadToWisdom, Error, TEXT("[ROUTE] no pawn to drive"));
			return;
		}

		State.StartSeconds = FPlatformTime::Seconds();
		TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&TickTicker), 0.0f);

		UE_LOG(LogRoadToWisdom, Warning, TEXT("[ROUTE] driving %d points, %.0f m"), State.Points.Num(), RouteLengthM());
	}
}

static FAutoConsoleCommand GQRWRouteDrive(
	TEXT("QRW.RouteDrive"),
	TEXT("Drives the tagged van along the Main road route; writes Saved/WorldGeneration/VanRouteMain.json"),
	FConsoleCommandWithArgsDelegate::CreateStatic(&QRWRouteDrive::StartRouteDrive));
