// Copyright 2017-2025 Razer, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#include "HapticVoicePool.h"
#include "IInterhapticsRuntime.h"
#include "WyvrnHapticRuntime.h"
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
		TMap<int32, float> LastIntensity;

		virtual bool Initialize() override { return true; }
		virtual void Shutdown() override {}
		virtual bool IsAvailable() const override { return true; }
		virtual int32 AddMaterial(const FString& MaterialJson) override { return NextId++; }
		virtual void SetIntensity(int32 MaterialId, float Intensity) override { LastIntensity.FindOrAdd(MaterialId) = Intensity; }
		virtual void SetLoop(int32 MaterialId, int32 NumLoops) override {}
		virtual void AddTarget(int32 MaterialId, EWyvrnHapticTarget Region, EWyvrnHapticSide Side) override {}
		virtual void Play(int32 MaterialId, double TimeSeconds) override { Played.Add(MaterialId); }
		virtual void Stop(int32 MaterialId) override { Stopped.Add(MaterialId); }
		virtual void StopAll() override { ++StoppedAllCount; }
		virtual double GetLength(int32 MaterialId) const override { return 1.0; }
		virtual void Render(double TimeSeconds) override {}
	};

	FWyvrnRuntimeEvent MakeEvent(int32 EffectId, int32 Loop, EWyvrnHapticPriority Priority, EWyvrnHapticMixing Mixing)
	{
		FWyvrnRuntimeEvent Event;
		Event.EffectId = EffectId;
		Event.Gain = 1.0f;
		Event.Loop = Loop;
		Event.Priority = Priority;
		Event.Mixing = Mixing;
		FWyvrnHapticTarget Target;
		Target.Region = EWyvrnHapticTarget::Hand;
		Target.Side = EWyvrnHapticSide::Global;
		Event.Targets.Add(Target);
		return Event;
	}

	FWyvrnRuntimeCommand MakePlay(const TCHAR* Name, int32 EffectId, int32 Loop, EWyvrnHapticPriority Priority, EWyvrnHapticMixing Mixing)
	{
		FWyvrnRuntimeCommand Command;
		Command.EventName = Name;
		Command.Effects.Add(MakeEvent(EffectId, Loop, Priority, Mixing));
		return Command;
	}

	FWyvrnRuntimeCommand MakeStop(const TCHAR* Name, const TCHAR* Target)
	{
		FWyvrnRuntimeCommand Command;
		Command.EventName = Name;
		Command.InterruptCommands.Add(Target);
		return Command;
	}

	// The bare-string "All" stop-all form (vs MakeStop which lists a literal event name).
	FWyvrnRuntimeCommand MakeStopAll(const TCHAR* Name)
	{
		FWyvrnRuntimeCommand Command;
		Command.EventName = Name;
		Command.bInterruptAll = true;
		return Command;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHapticVoicePoolTest,
	"Wyvrn.Haptics.VoicePool",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHapticVoicePoolTest::RunTest(const FString& Parameters)
{
	FWyvrnRuntimeData Data;
	Data.EffectJson.Add(TEXT("{}"));
	Data.Commands.Add(MakePlay(TEXT("Effect1"), 0, 0, EWyvrnHapticPriority::High, EWyvrnHapticMixing::Merge));

	FMockInterhapticsRuntime Runtime;
	FHapticVoicePool Pool(Runtime, 2);
	Pool.Preload(Data);

	// Three concurrent plays of the same effect with a 2-voice pool: two distinct
	// voices, then a stolen reuse — never a silent restart of a single id.
	Pool.PlayCommand(Data.Commands[0], 0.0);
	Pool.PlayCommand(Data.Commands[0], 0.0);
	Pool.PlayCommand(Data.Commands[0], 0.0);

	TestEqual(TEXT("three plays issued"), Runtime.Played.Num(), 3);
	if (Runtime.Played.Num() == 3)
	{
		TestEqual(TEXT("first voice"), Runtime.Played[0], 1);
		TestEqual(TEXT("second voice gives polyphony"), Runtime.Played[1], 2);
		TestEqual(TEXT("third reuses a stolen voice"), Runtime.Played[2], 1);
	}

	// A command interrupting "Effect1" stops the voices it currently owns.
	FWyvrnRuntimeCommand Stopper = MakeStop(TEXT("Stop"), TEXT("Effect1"));
	Pool.PlayCommand(Stopper, 0.0);

	TestTrue(TEXT("interrupt stopped voice 1"), Runtime.Stopped.Contains(1));
	TestTrue(TEXT("interrupt stopped voice 2"), Runtime.Stopped.Contains(2));

	// After the playback length elapses, voices free up and reclaim cleanly.
	Pool.Tick(5.0);
	Pool.PlayCommand(Data.Commands[0], 5.0);
	TestEqual(TEXT("a play after reclaim issues one more"), Runtime.Played.Num(), 4);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHapticVoicePoolStopAllTest,
	"Wyvrn.Haptics.VoicePoolStopAll",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHapticVoicePoolStopAllTest::RunTest(const FString& Parameters)
{
	FWyvrnRuntimeData Data;
	Data.EffectJson.Add(TEXT("{}"));
	Data.Commands.Add(MakePlay(TEXT("Effect1"), 0, 0, EWyvrnHapticPriority::High, EWyvrnHapticMixing::Merge));

	FMockInterhapticsRuntime Runtime;
	FHapticVoicePool Pool(Runtime, 2);
	Pool.Preload(Data);

	Pool.PlayCommand(Data.Commands[0], 0.0);

	// A command flagged bInterruptAll (the bare-string "All" form) stops every active
	// event through StopAll, rather than looking up an event named "All".
	FWyvrnRuntimeCommand StopAll = MakeStopAll(TEXT("Stop"));
	Pool.PlayCommand(StopAll, 0.0);

	TestEqual(TEXT("\"All\" interrupt calls StopAll once"), Runtime.StoppedAllCount, 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHapticVoicePoolPriorityDuckTest,
	"Wyvrn.Haptics.VoicePoolPriorityDuck",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHapticVoicePoolPriorityDuckTest::RunTest(const FString& Parameters)
{
	// Two looping effects of different priority. A (High) silences B (Low) while it
	// plays; stopping A un-ducks B mid-stream.
	FWyvrnRuntimeData Data;
	Data.EffectJson.Add(TEXT("{}")); // EffectId 0 -> A
	Data.EffectJson.Add(TEXT("{}")); // EffectId 1 -> B
	Data.Commands.Add(MakePlay(TEXT("A"), 0, -1, EWyvrnHapticPriority::High, EWyvrnHapticMixing::Merge));
	Data.Commands.Add(MakePlay(TEXT("B"), 1, -1, EWyvrnHapticPriority::Low, EWyvrnHapticMixing::Merge));
	Data.Commands.Add(MakeStop(TEXT("StopA"), TEXT("A")));

	FMockInterhapticsRuntime Runtime;
	FHapticVoicePool Pool(Runtime, 1);
	Pool.Preload(Data);
	// One voice each, preloaded in command order: A -> id 1, B -> id 2.

	Pool.PlayCommand(Data.Commands[0], 0.0); // A (High)
	Pool.PlayCommand(Data.Commands[1], 0.0); // B (Low) -> ducked under A

	TestEqual(TEXT("A audible at full gain"), Runtime.LastIntensity.FindRef(1), 1.0f);
	TestEqual(TEXT("B ducked to silence under higher-priority A"), Runtime.LastIntensity.FindRef(2), 0.0f);

	Pool.PlayCommand(Data.Commands[2], 0.0); // StopA
	TestTrue(TEXT("A stopped"), Runtime.Stopped.Contains(1));
	TestEqual(TEXT("B restored to full gain once A stops"), Runtime.LastIntensity.FindRef(2), 1.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHapticVoicePoolMixingTest,
	"Wyvrn.Haptics.VoicePoolMixing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHapticVoicePoolMixingTest::RunTest(const FString& Parameters)
{
	// Equal-priority events. Merge plays both; Override makes the newest play alone
	// and silences the older one, which resumes when the newer stops.
	FWyvrnRuntimeData Data;
	Data.EffectJson.Add(TEXT("{}")); // EffectId 0 -> A
	Data.EffectJson.Add(TEXT("{}")); // EffectId 1 -> B (shared by BMerge/BOverride)
	Data.Commands.Add(MakePlay(TEXT("A"), 0, -1, EWyvrnHapticPriority::Medium, EWyvrnHapticMixing::Merge));
	Data.Commands.Add(MakePlay(TEXT("BMerge"), 1, -1, EWyvrnHapticPriority::Medium, EWyvrnHapticMixing::Merge));
	Data.Commands.Add(MakePlay(TEXT("BOverride"), 1, -1, EWyvrnHapticPriority::Medium, EWyvrnHapticMixing::Override));
	Data.Commands.Add(MakeStop(TEXT("StopB"), TEXT("BOverride")));
	Data.Commands.Add(MakeStopAll(TEXT("StopAll")));

	FMockInterhapticsRuntime Runtime;
	FHapticVoicePool Pool(Runtime, 1);
	Pool.Preload(Data);
	// A -> id 1, B -> id 2.

	// Merge: both equal-priority events stay audible.
	Pool.PlayCommand(Data.Commands[0], 0.0); // A
	Pool.PlayCommand(Data.Commands[1], 0.0); // BMerge
	TestEqual(TEXT("A audible when merging"), Runtime.LastIntensity.FindRef(1), 1.0f);
	TestEqual(TEXT("B audible when merging"), Runtime.LastIntensity.FindRef(2), 1.0f);

	// Reset and exercise Override.
	Pool.PlayCommand(Data.Commands[4], 0.0); // StopAll

	Pool.PlayCommand(Data.Commands[0], 0.0); // A again -> id 1
	Pool.PlayCommand(Data.Commands[2], 0.0); // BOverride -> id 2, overrides A
	TestEqual(TEXT("A silenced by newer Override at equal priority"), Runtime.LastIntensity.FindRef(1), 0.0f);
	TestEqual(TEXT("Override event audible"), Runtime.LastIntensity.FindRef(2), 1.0f);

	Pool.PlayCommand(Data.Commands[3], 0.0); // StopB (the override)
	TestEqual(TEXT("A resumes once the Override stops"), Runtime.LastIntensity.FindRef(1), 1.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHapticVoicePoolLoopStopReuseTest,
	"Wyvrn.Haptics.VoicePoolLoopStopReuse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHapticVoicePoolLoopStopReuseTest::RunTest(const FString& Parameters)
{
	// Regression for the looping-voice leak: a looping voice stopped via interrupt must
	// become free again (not stay permanently busy). LoopHi (loop) and OneShotLo (one-shot)
	// share one 2-voice pool so the freed loop voice can be observed being reused.
	FWyvrnRuntimeData Data;
	Data.EffectJson.Add(TEXT("{}"));
	Data.Commands.Add(MakePlay(TEXT("LoopHi"), 0, -1, EWyvrnHapticPriority::High, EWyvrnHapticMixing::Merge));
	Data.Commands.Add(MakePlay(TEXT("OneShotLo"), 0, 0, EWyvrnHapticPriority::Low, EWyvrnHapticMixing::Merge));
	Data.Commands.Add(MakeStop(TEXT("StopLoop"), TEXT("LoopHi")));

	FMockInterhapticsRuntime Runtime;
	FHapticVoicePool Pool(Runtime, 2);
	Pool.Preload(Data);

	Pool.PlayCommand(Data.Commands[0], 0.0); // LoopHi -> voice 1 (looping, High)
	Pool.PlayCommand(Data.Commands[1], 0.0); // OneShotLo -> voice 2 (one-shot, Low)

	Pool.PlayCommand(Data.Commands[2], 0.0); // StopLoop -> stops & frees voice 1
	TestTrue(TEXT("looping voice stopped"), Runtime.Stopped.Contains(1));

	// The freed loop voice 1 must be reused next. Without the leak fix it would still read
	// as busy (bLooping/BusyUntil=Max), forcing the play to steal the Low voice 2 instead.
	Pool.PlayCommand(Data.Commands[1], 0.0); // OneShotLo again
	TestEqual(TEXT("three plays total"), Runtime.Played.Num(), 3);
	if (Runtime.Played.Num() == 3)
	{
		TestEqual(TEXT("freed looping voice 1 is reused, not leaked"), Runtime.Played[2], 1);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHapticVoicePoolLoopCoalesceTest,
	"Wyvrn.Haptics.VoicePoolLoopCoalesce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHapticVoicePoolLoopCoalesceTest::RunTest(const FString& Parameters)
{
	// Re-triggering a looping event restarts its single voice instead of stacking new
	// ones, so spamming a loop cannot pile up concurrent voices (the PS5 frame-drop fix).
	FWyvrnRuntimeData Data;
	Data.EffectJson.Add(TEXT("{}"));
	Data.Commands.Add(MakePlay(TEXT("Loop"), 0, -1, EWyvrnHapticPriority::High, EWyvrnHapticMixing::Merge));

	FMockInterhapticsRuntime Runtime;
	FHapticVoicePool Pool(Runtime, 4);
	Pool.Preload(Data);

	for (int32 Index = 0; Index < 5; ++Index)
	{
		Pool.PlayCommand(Data.Commands[0], 0.0);
	}

	TestEqual(TEXT("five plays issued"), Runtime.Played.Num(), 5);
	bool bAllSameVoice = Runtime.Played.Num() == 5;
	for (const int32 Id : Runtime.Played)
	{
		bAllSameVoice = bAllSameVoice && (Id == 1);
	}
	TestTrue(TEXT("all loop re-triggers reuse the one voice (no pile-up)"), bAllSameVoice);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
