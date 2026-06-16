// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#include "HapticVoicePool.h"
#include "IInterhapticsRuntime.h"
#include "WyvrnHapticData.h"
#include "WyvrnHapticEffect.h"
#include "WyvrnHapticTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	// Records the HAR calls the pool issues so the test can assert on them.
	class FMockInterhapticsRuntime : public IInterhapticsRuntime
	{
	public:
		int32 NextId = 1;
		TArray<int32> Played;
		TArray<int32> Stopped;
		int32 StoppedAllCount = 0;

		virtual bool Initialize() override { return true; }
		virtual void Shutdown() override {}
		virtual bool IsAvailable() const override { return true; }
		virtual int32 AddMaterial(const FString& MaterialJson) override { return NextId++; }
		virtual void SetIntensity(int32 MaterialId, float Intensity) override {}
		virtual void SetLoop(int32 MaterialId, int32 NumLoops) override {}
		virtual void AddTarget(int32 MaterialId, EWyvrnHapticTarget Target) override {}
		virtual void Play(int32 MaterialId, double TimeSeconds) override { Played.Add(MaterialId); }
		virtual void Stop(int32 MaterialId) override { Stopped.Add(MaterialId); }
		virtual void StopAll() override { ++StoppedAllCount; }
		virtual double GetLength(int32 MaterialId) const override { return 1.0; }
		virtual void Render(double TimeSeconds) override {}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHapticVoicePoolTest,
	"Wyvrn.Haptics.VoicePool",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHapticVoicePoolTest::RunTest(const FString& Parameters)
{
	UWyvrnHapticEffect* HapticEffect = NewObject<UWyvrnHapticEffect>(GetTransientPackage());
	HapticEffect->Json = TEXT("{}");

	FWyvrnHapticEvent EffectEntry;
	EffectEntry.Effect = HapticEffect;
	EffectEntry.Gain = 1.0f;
	EffectEntry.Loop = 0;
	EffectEntry.Priority = EWyvrnHapticPriority::High;
	EffectEntry.Targets.Add(EWyvrnHapticTarget::Hand);

	FWyvrnHapticCommand Command;
	Command.EventName = TEXT("Effect1");
	Command.Effects.Add(EffectEntry);

	UWyvrnHapticData* Data = NewObject<UWyvrnHapticData>(GetTransientPackage());
	Data->Commands.Add(Command);

	FMockInterhapticsRuntime Runtime;
	FHapticVoicePool Pool(Runtime, 2);
	Pool.Preload(*Data);

	// Three concurrent plays of the same effect with a 2-voice pool: two distinct
	// voices, then a stolen reuse — never a silent restart of a single id.
	Pool.PlayCommand(Command, 0.0);
	Pool.PlayCommand(Command, 0.0);
	Pool.PlayCommand(Command, 0.0);

	TestEqual(TEXT("three plays issued"), Runtime.Played.Num(), 3);
	if (Runtime.Played.Num() == 3)
	{
		TestEqual(TEXT("first voice"), Runtime.Played[0], 1);
		TestEqual(TEXT("second voice gives polyphony"), Runtime.Played[1], 2);
		TestEqual(TEXT("third reuses a stolen voice"), Runtime.Played[2], 1);
	}

	// A command interrupting "Effect1" stops the voices it currently owns.
	FWyvrnHapticCommand Stopper;
	Stopper.EventName = TEXT("Stop");
	Stopper.InterruptCommands.Add(TEXT("Effect1"));
	Pool.PlayCommand(Stopper, 0.0);

	TestTrue(TEXT("interrupt stopped voice 1"), Runtime.Stopped.Contains(1));
	TestTrue(TEXT("interrupt stopped voice 2"), Runtime.Stopped.Contains(2));

	// After the playback length elapses, voices free up and reclaim cleanly.
	Pool.Tick(5.0);
	Pool.PlayCommand(Command, 5.0);
	TestEqual(TEXT("a play after reclaim issues one more"), Runtime.Played.Num(), 4);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHapticVoicePoolStopAllTest,
	"Wyvrn.Haptics.VoicePoolStopAll",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHapticVoicePoolStopAllTest::RunTest(const FString& Parameters)
{
	UWyvrnHapticEffect* HapticEffect = NewObject<UWyvrnHapticEffect>(GetTransientPackage());
	HapticEffect->Json = TEXT("{}");

	FWyvrnHapticEvent EffectEntry;
	EffectEntry.Effect = HapticEffect;
	EffectEntry.Gain = 1.0f;
	EffectEntry.Loop = 0;
	EffectEntry.Priority = EWyvrnHapticPriority::High;
	EffectEntry.Targets.Add(EWyvrnHapticTarget::Hand);

	FWyvrnHapticCommand Command;
	Command.EventName = TEXT("Effect1");
	Command.Effects.Add(EffectEntry);

	UWyvrnHapticData* Data = NewObject<UWyvrnHapticData>(GetTransientPackage());
	Data->Commands.Add(Command);

	FMockInterhapticsRuntime Runtime;
	FHapticVoicePool Pool(Runtime, 2);
	Pool.Preload(*Data);

	Pool.PlayCommand(Command, 0.0);

	// A command whose interrupt list carries the "All" sentinel stops every active
	// event through StopAll, rather than looking up an event named "All".
	FWyvrnHapticCommand StopAll;
	StopAll.EventName = TEXT("Stop");
	StopAll.InterruptCommands.Add(TEXT("All"));
	Pool.PlayCommand(StopAll, 0.0);

	TestEqual(TEXT("\"All\" interrupt calls StopAll once"), Runtime.StoppedAllCount, 1);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
