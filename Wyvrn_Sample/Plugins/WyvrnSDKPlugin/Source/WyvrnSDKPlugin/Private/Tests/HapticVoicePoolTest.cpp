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
		/** Last SetGlobalIntensity factor; -1 means it was never called. */
		float LastGlobalIntensity = -1.0f;
		/** (material id, bLeftTrigger) per StartTriggerEffect call, in order. */
		TArray<TPair<int32, bool>> TriggerStarts;
		/** bLeftTrigger per StopTriggerEffect call, in order. */
		TArray<bool> TriggerStops;
		/** Fail this many upcoming StartTriggerEffect calls (simulates a pad-rejected arm). */
		int32 FailTriggerStarts = 0;
		/** Per-material GetLength override, in seconds; unlisted materials return 1.0. */
		TMap<int32, double> LengthOverrides;

		virtual bool Initialize() override { return true; }
		virtual void Shutdown() override {}
		virtual bool IsAvailable() const override { return true; }
		virtual int32 AddMaterial(const FString& MaterialJson) override { return NextId++; }
		virtual void SetIntensity(int32 MaterialId, float Intensity) override { LastIntensity.FindOrAdd(MaterialId) = Intensity; }
		virtual void SetGlobalIntensity(float Intensity) override { LastGlobalIntensity = Intensity; }
		virtual void SetLoop(int32 MaterialId, int32 NumLoops) override {}
		virtual void AddTarget(int32 MaterialId, EWyvrnHapticTarget Region, EWyvrnHapticSide Side) override {}
		virtual void Play(int32 MaterialId, double TimeSeconds) override { Played.Add(MaterialId); }
		virtual void Stop(int32 MaterialId) override { Stopped.Add(MaterialId); }
		virtual void StopAll() override { ++StoppedAllCount; }
		virtual double GetLength(int32 MaterialId) const override
		{
			const double* Length = LengthOverrides.Find(MaterialId);
			return Length != nullptr ? *Length : 1.0;
		}
		virtual void Render(double TimeSeconds) override {}
		virtual bool StartTriggerEffect(int32 MaterialId, bool bLeftTrigger) override
		{
			TriggerStarts.Emplace(MaterialId, bLeftTrigger);
			if (FailTriggerStarts > 0)
			{
				--FailTriggerStarts;
				return false;
			}
			return true;
		}
		virtual void StopTriggerEffect(bool bLeftTrigger) override { TriggerStops.Add(bLeftTrigger); }
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

	// A play command whose effect carries a Stiffness track, hand-targeted on Side.
	FWyvrnRuntimeCommand MakeStiffnessPlay(const TCHAR* Name, int32 EffectId, int32 Loop, EWyvrnHapticSide Side)
	{
		FWyvrnRuntimeCommand Command = MakePlay(Name, EffectId, Loop, EWyvrnHapticPriority::High, EWyvrnHapticMixing::Merge);
		Command.Effects[0].bHasStiffness = true;
		Command.Effects[0].Targets[0].Side = Side;
		return Command;
	}

	int32 CountTriggerStarts(const FMockInterhapticsRuntime& Runtime, int32 MaterialId, bool bLeftTrigger)
	{
		int32 Count = 0;
		for (const TPair<int32, bool>& Start : Runtime.TriggerStarts)
		{
			if (Start.Key == MaterialId && Start.Value == bLeftTrigger)
			{
				++Count;
			}
		}
		return Count;
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHapticVoicePoolTriggerStartStopTest,
	"Wyvrn.Haptics.VoicePoolTriggerStartStop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHapticVoicePoolTriggerStartStopTest::RunTest(const FString& Parameters)
{
	// A stiffness-carrying event claims the adaptive triggers while it plays: a
	// Global hand side arms BOTH triggers with its material; an interrupt releases
	// them. An effect without a Stiffness track never touches the triggers.
	FWyvrnRuntimeData Data;
	Data.EffectJson.Add(TEXT("{}")); // EffectId 0 -> stiffness-carrying "On"
	Data.EffectJson.Add(TEXT("{}")); // EffectId 1 -> plain vibration
	Data.Commands.Add(MakeStiffnessPlay(TEXT("On"), 0, -1, EWyvrnHapticSide::Global));
	Data.Commands.Add(MakePlay(TEXT("Plain"), 1, 0, EWyvrnHapticPriority::High, EWyvrnHapticMixing::Merge));
	Data.Commands.Add(MakeStop(TEXT("Off"), TEXT("On")));

	FMockInterhapticsRuntime Runtime;
	FHapticVoicePool Pool(Runtime, 1);
	Pool.Preload(Data);
	// On -> material 1, Plain -> material 2.

	Pool.PlayCommand(Data.Commands[0], 0.0); // On
	TestEqual(TEXT("both triggers armed by the Global stiffness event"), Runtime.TriggerStarts.Num(), 2);
	TestEqual(TEXT("L2 armed with the event's material"), CountTriggerStarts(Runtime, 1, true), 1);
	TestEqual(TEXT("R2 armed with the event's material"), CountTriggerStarts(Runtime, 1, false), 1);

	Pool.PlayCommand(Data.Commands[1], 0.0); // Plain
	TestEqual(TEXT("a stiffness-less event arms nothing"), Runtime.TriggerStarts.Num(), 2);
	TestEqual(TEXT("a stiffness-less event releases nothing"), Runtime.TriggerStops.Num(), 0);

	Pool.PlayCommand(Data.Commands[2], 0.0); // Off (interrupts On)
	TestTrue(TEXT("interrupt stopped the stiffness voice"), Runtime.Stopped.Contains(1));
	TestEqual(TEXT("both triggers released on interrupt"), Runtime.TriggerStops.Num(), 2);
	TestTrue(TEXT("L2 released"), Runtime.TriggerStops.Contains(true));
	TestTrue(TEXT("R2 released"), Runtime.TriggerStops.Contains(false));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHapticVoicePoolTriggerLateralEndTest,
	"Wyvrn.Haptics.VoicePoolTriggerLateralEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHapticVoicePoolTriggerLateralEndTest::RunTest(const FString& Parameters)
{
	// A Left-sided stiffness one-shot arms only L2, and the latch OUTLIVES the
	// voice: the natural end of playback releases nothing; only the interrupt does.
	FWyvrnRuntimeData Data;
	Data.EffectJson.Add(TEXT("{}"));
	Data.Commands.Add(MakeStiffnessPlay(TEXT("FireL"), 0, 0, EWyvrnHapticSide::Left));
	Data.Commands.Add(MakeStop(TEXT("Off"), TEXT("FireL")));

	FMockInterhapticsRuntime Runtime;
	FHapticVoicePool Pool(Runtime, 1);
	Pool.Preload(Data);

	Pool.PlayCommand(Data.Commands[0], 0.0);
	TestEqual(TEXT("only one trigger armed"), Runtime.TriggerStarts.Num(), 1);
	TestEqual(TEXT("L2 armed"), CountTriggerStarts(Runtime, 1, true), 1);
	TestEqual(TEXT("R2 untouched"), CountTriggerStarts(Runtime, 1, false), 0);

	Pool.Tick(5.0); // past the mock's 1s length: the one-shot's VOICE retires...
	TestEqual(TEXT("latch persists past natural end"), Runtime.TriggerStops.Num(), 0);

	Pool.PlayCommand(Data.Commands[1], 0.0); // ...but only the authored OFF releases
	TestEqual(TEXT("one release on interrupt"), Runtime.TriggerStops.Num(), 1);
	TestTrue(TEXT("L2 released"), Runtime.TriggerStops.Num() == 1 && Runtime.TriggerStops[0]);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHapticVoicePoolTriggerHandoffTest,
	"Wyvrn.Haptics.VoicePoolTriggerHandoff",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHapticVoicePoolTriggerHandoffTest::RunTest(const FString& Parameters)
{
	// Overlapping stiffness events: the newest claimant owns the trigger; stopping
	// it hands the trigger back to the survivor (a re-arm, not a release); stop-all
	// releases everything.
	FWyvrnRuntimeData Data;
	Data.EffectJson.Add(TEXT("{}")); // EffectId 0 -> A
	Data.EffectJson.Add(TEXT("{}")); // EffectId 1 -> B
	Data.Commands.Add(MakeStiffnessPlay(TEXT("A"), 0, -1, EWyvrnHapticSide::Global));
	Data.Commands.Add(MakeStiffnessPlay(TEXT("B"), 1, -1, EWyvrnHapticSide::Global));
	Data.Commands.Add(MakeStop(TEXT("StopB"), TEXT("B")));
	Data.Commands.Add(MakeStopAll(TEXT("StopAll")));

	FMockInterhapticsRuntime Runtime;
	FHapticVoicePool Pool(Runtime, 1);
	Pool.Preload(Data);
	// A -> material 1, B -> material 2.

	Pool.PlayCommand(Data.Commands[0], 0.0); // A owns both triggers
	TestEqual(TEXT("A arms L2"), CountTriggerStarts(Runtime, 1, true), 1);
	TestEqual(TEXT("A arms R2"), CountTriggerStarts(Runtime, 1, false), 1);

	Pool.PlayCommand(Data.Commands[1], 0.0); // B is newer -> takes both triggers over
	TestEqual(TEXT("B re-arms L2"), CountTriggerStarts(Runtime, 2, true), 1);
	TestEqual(TEXT("B re-arms R2"), CountTriggerStarts(Runtime, 2, false), 1);
	TestEqual(TEXT("takeover is a re-arm, not a release"), Runtime.TriggerStops.Num(), 0);

	Pool.PlayCommand(Data.Commands[2], 0.0); // StopB -> triggers hand back to A
	TestEqual(TEXT("L2 handed back to A"), CountTriggerStarts(Runtime, 1, true), 2);
	TestEqual(TEXT("R2 handed back to A"), CountTriggerStarts(Runtime, 1, false), 2);
	TestEqual(TEXT("handoff is a re-arm, not a release"), Runtime.TriggerStops.Num(), 0);

	Pool.PlayCommand(Data.Commands[3], 0.0); // StopAll -> no claimant left
	TestEqual(TEXT("both triggers released by stop-all"), Runtime.TriggerStops.Num(), 2);
	TestTrue(TEXT("L2 released"), Runtime.TriggerStops.Contains(true));
	TestTrue(TEXT("R2 released"), Runtime.TriggerStops.Contains(false));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHapticVoicePoolTriggerRightSideTest,
	"Wyvrn.Haptics.VoicePoolTriggerRightSide",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHapticVoicePoolTriggerRightSideTest::RunTest(const FString& Parameters)
{
	// The Right-side mirror of the lateral test: only R2 is armed, and only the
	// authored OFF releases it.
	FWyvrnRuntimeData Data;
	Data.EffectJson.Add(TEXT("{}"));
	Data.Commands.Add(MakeStiffnessPlay(TEXT("FireR"), 0, 0, EWyvrnHapticSide::Right));
	Data.Commands.Add(MakeStop(TEXT("Off"), TEXT("FireR")));

	FMockInterhapticsRuntime Runtime;
	FHapticVoicePool Pool(Runtime, 1);
	Pool.Preload(Data);

	Pool.PlayCommand(Data.Commands[0], 0.0);
	TestEqual(TEXT("only one trigger armed"), Runtime.TriggerStarts.Num(), 1);
	TestEqual(TEXT("R2 armed"), CountTriggerStarts(Runtime, 1, false), 1);
	TestEqual(TEXT("L2 untouched"), CountTriggerStarts(Runtime, 1, true), 0);

	Pool.Tick(5.0);
	TestEqual(TEXT("latch persists past natural end"), Runtime.TriggerStops.Num(), 0);

	Pool.PlayCommand(Data.Commands[1], 0.0);
	TestEqual(TEXT("one release on interrupt"), Runtime.TriggerStops.Num(), 1);
	TestTrue(TEXT("R2 released"), Runtime.TriggerStops.Num() == 1 && !Runtime.TriggerStops[0]);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHapticVoicePoolTriggerPartialTest,
	"Wyvrn.Haptics.VoicePoolTriggerPartial",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHapticVoicePoolTriggerPartialTest::RunTest(const FString& Parameters)
{
	// Partial laterality: a newer Left-only claim takes L2 but leaves R2 with the
	// older Global claim; stopping it hands only L2 back.
	FWyvrnRuntimeData Data;
	Data.EffectJson.Add(TEXT("{}")); // EffectId 0 -> A (Global)
	Data.EffectJson.Add(TEXT("{}")); // EffectId 1 -> C (Left)
	Data.Commands.Add(MakeStiffnessPlay(TEXT("A"), 0, -1, EWyvrnHapticSide::Global));
	Data.Commands.Add(MakeStiffnessPlay(TEXT("C"), 1, -1, EWyvrnHapticSide::Left));
	Data.Commands.Add(MakeStop(TEXT("StopC"), TEXT("C")));
	Data.Commands.Add(MakeStopAll(TEXT("StopAll")));

	FMockInterhapticsRuntime Runtime;
	FHapticVoicePool Pool(Runtime, 1);
	Pool.Preload(Data);
	// A -> material 1, C -> material 2.

	Pool.PlayCommand(Data.Commands[0], 0.0); // A owns both
	Pool.PlayCommand(Data.Commands[1], 0.0); // C takes L2 only
	TestEqual(TEXT("C armed L2"), CountTriggerStarts(Runtime, 2, true), 1);
	TestEqual(TEXT("C never touches R2"), CountTriggerStarts(Runtime, 2, false), 0);
	TestEqual(TEXT("R2 not re-armed (still A's)"), CountTriggerStarts(Runtime, 1, false), 1);
	TestEqual(TEXT("no releases during takeover"), Runtime.TriggerStops.Num(), 0);

	Pool.PlayCommand(Data.Commands[2], 0.0); // StopC -> L2 back to A, R2 untouched
	TestEqual(TEXT("L2 handed back to A"), CountTriggerStarts(Runtime, 1, true), 2);
	TestEqual(TEXT("R2 still not re-armed"), CountTriggerStarts(Runtime, 1, false), 1);
	TestEqual(TEXT("handoff is a re-arm, not a release"), Runtime.TriggerStops.Num(), 0);

	Pool.PlayCommand(Data.Commands[3], 0.0); // StopAll
	TestEqual(TEXT("both triggers released"), Runtime.TriggerStops.Num(), 2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHapticVoicePoolTriggerCoalesceNoRearmTest,
	"Wyvrn.Haptics.VoicePoolTriggerCoalesceNoRearm",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHapticVoicePoolTriggerCoalesceNoRearmTest::RunTest(const FString& Parameters)
{
	// Re-triggering a live looping stiffness event coalesces into its own voice and
	// must NOT re-arm the pad every time (the owner material is unchanged).
	FWyvrnRuntimeData Data;
	Data.EffectJson.Add(TEXT("{}"));
	Data.Commands.Add(MakeStiffnessPlay(TEXT("Loop"), 0, -1, EWyvrnHapticSide::Global));

	FMockInterhapticsRuntime Runtime;
	FHapticVoicePool Pool(Runtime, 4);
	Pool.Preload(Data);

	for (int32 Index = 0; Index < 3; ++Index)
	{
		Pool.PlayCommand(Data.Commands[0], 0.0);
	}

	TestEqual(TEXT("three plays coalesce into one voice"), Runtime.Played.Num(), 3);
	TestEqual(TEXT("triggers armed exactly once per side"), Runtime.TriggerStarts.Num(), 2);
	TestEqual(TEXT("no spurious releases"), Runtime.TriggerStops.Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHapticVoicePoolTriggerDtorReleaseTest,
	"Wyvrn.Haptics.VoicePoolTriggerDtorRelease",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHapticVoicePoolTriggerDtorReleaseTest::RunTest(const FString& Parameters)
{
	// The applied resistance outlives playback on the pad, so tearing the pool down
	// mid-claim must release every armed trigger.
	FWyvrnRuntimeData Data;
	Data.EffectJson.Add(TEXT("{}"));
	Data.Commands.Add(MakeStiffnessPlay(TEXT("On"), 0, -1, EWyvrnHapticSide::Global));

	FMockInterhapticsRuntime Runtime;
	{
		FHapticVoicePool Pool(Runtime, 1);
		Pool.Preload(Data);
		Pool.PlayCommand(Data.Commands[0], 0.0);
		TestEqual(TEXT("armed before teardown"), Runtime.TriggerStarts.Num(), 2);
		TestEqual(TEXT("nothing released before teardown"), Runtime.TriggerStops.Num(), 0);
	}

	TestEqual(TEXT("both triggers released by the destructor"), Runtime.TriggerStops.Num(), 2);
	TestTrue(TEXT("L2 released"), Runtime.TriggerStops.Contains(true));
	TestTrue(TEXT("R2 released"), Runtime.TriggerStops.Contains(false));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHapticVoicePoolTriggerDuckKeepTest,
	"Wyvrn.Haptics.VoicePoolTriggerDuckKeep",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHapticVoicePoolTriggerDuckKeepTest::RunTest(const FString& Parameters)
{
	// Priority ducking silences a voice's vibration but must not disturb its
	// adaptive-trigger claim: no release or re-arm across duck and un-duck.
	FWyvrnRuntimeData Data;
	Data.EffectJson.Add(TEXT("{}")); // EffectId 0 -> stiffness A (High, from the helper)
	Data.EffectJson.Add(TEXT("{}")); // EffectId 1 -> dominant plain B (VeryHigh)
	Data.Commands.Add(MakeStiffnessPlay(TEXT("A"), 0, -1, EWyvrnHapticSide::Global));
	Data.Commands.Add(MakePlay(TEXT("B"), 1, -1, EWyvrnHapticPriority::VeryHigh, EWyvrnHapticMixing::Merge));
	Data.Commands.Add(MakeStop(TEXT("StopB"), TEXT("B")));

	FMockInterhapticsRuntime Runtime;
	FHapticVoicePool Pool(Runtime, 1);
	Pool.Preload(Data);
	// A -> material 1, B -> material 2.

	Pool.PlayCommand(Data.Commands[0], 0.0); // A armed
	Pool.PlayCommand(Data.Commands[1], 0.0); // B ducks A's vibration
	TestEqual(TEXT("A's vibration ducked"), Runtime.LastIntensity.FindRef(1), 0.0f);
	TestEqual(TEXT("claim kept while ducked (no extra arms)"), Runtime.TriggerStarts.Num(), 2);
	TestEqual(TEXT("claim kept while ducked (no releases)"), Runtime.TriggerStops.Num(), 0);

	Pool.PlayCommand(Data.Commands[2], 0.0); // StopB un-ducks A
	TestEqual(TEXT("A's vibration restored"), Runtime.LastIntensity.FindRef(1), 1.0f);
	TestEqual(TEXT("no trigger churn across un-duck"), Runtime.TriggerStarts.Num(), 2);
	TestEqual(TEXT("still no releases"), Runtime.TriggerStops.Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHapticVoicePoolTriggerLatchOutlivesVoiceTest,
	"Wyvrn.Haptics.VoicePoolTriggerLatchOutlivesVoice",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHapticVoicePoolTriggerLatchOutlivesVoiceTest::RunTest(const FString& Parameters)
{
	// The latch is independent of the voice, so it must survive the voice's complete
	// retirement (a stiffness-only .haps: GetLength 0, the voice retires on the very
	// next Tick), replays must re-assert without re-arming the pad, and the authored
	// OFF must release it even though no voice is live anymore.
	FWyvrnRuntimeData Data;
	Data.EffectJson.Add(TEXT("{}"));
	Data.Commands.Add(MakeStiffnessPlay(TEXT("Bow"), 0, 0, EWyvrnHapticSide::Global));
	Data.Commands.Add(MakeStop(TEXT("Release"), TEXT("Bow")));

	FMockInterhapticsRuntime Runtime;
	FHapticVoicePool Pool(Runtime, 2);
	Pool.Preload(Data);
	Runtime.LengthOverrides.Add(1, 0.0); // materials are stiffness-only
	Runtime.LengthOverrides.Add(2, 0.0);

	Pool.PlayCommand(Data.Commands[0], 1.0);
	TestEqual(TEXT("both triggers armed"), Runtime.TriggerStarts.Num(), 2);

	Pool.Tick(100.0); // the zero-length voice retires immediately...
	TestEqual(TEXT("voice retired"), Pool.GetActiveVoiceCount(), 0);
	TestEqual(TEXT("latch survives the voice"), Runtime.TriggerStops.Num(), 0);

	Pool.PlayCommand(Data.Commands[0], 101.0); // replay re-asserts the same material
	TestEqual(TEXT("no re-arm on replay of the same claim"), Runtime.TriggerStarts.Num(), 2);

	Pool.Tick(200.0);
	Pool.PlayCommand(Data.Commands[1], 201.0); // authored OFF, long after any voice ended
	TestEqual(TEXT("interrupt releases both triggers"), Runtime.TriggerStops.Num(), 2);
	TestTrue(TEXT("L2 released"), Runtime.TriggerStops.Contains(true));
	TestTrue(TEXT("R2 released"), Runtime.TriggerStops.Contains(false));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHapticVoicePoolTriggerArmRetryTest,
	"Wyvrn.Haptics.VoicePoolTriggerArmRetry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHapticVoicePoolTriggerArmRetryTest::RunTest(const FString& Parameters)
{
	// A pad-rejected arm (e.g. controller not connected yet) must not be recorded as
	// applied: the next lifecycle change retries it.
	FWyvrnRuntimeData Data;
	Data.EffectJson.Add(TEXT("{}")); // EffectId 0 -> stiffness A
	Data.EffectJson.Add(TEXT("{}")); // EffectId 1 -> plain B
	Data.Commands.Add(MakeStiffnessPlay(TEXT("A"), 0, -1, EWyvrnHapticSide::Global));
	Data.Commands.Add(MakePlay(TEXT("B"), 1, 0, EWyvrnHapticPriority::High, EWyvrnHapticMixing::Merge));
	Data.Commands.Add(MakeStop(TEXT("StopA"), TEXT("A")));

	FMockInterhapticsRuntime Runtime;
	FHapticVoicePool Pool(Runtime, 1);
	Pool.Preload(Data);
	// A -> material 1, B -> material 2.

	Runtime.FailTriggerStarts = 2; // both sides' first arm gets rejected
	Pool.PlayCommand(Data.Commands[0], 0.0);
	TestEqual(TEXT("both arms attempted"), Runtime.TriggerStarts.Num(), 2);

	Pool.PlayCommand(Data.Commands[1], 0.0); // any lifecycle change retries the arms
	TestEqual(TEXT("both arms retried"), CountTriggerStarts(Runtime, 1, true), 2);
	TestEqual(TEXT("both arms retried (right)"), CountTriggerStarts(Runtime, 1, false), 2);

	Pool.PlayCommand(Data.Commands[2], 0.0); // StopA releases the now-applied claims
	TestEqual(TEXT("released after successful retry"), Runtime.TriggerStops.Num(), 2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHapticVoicePoolStopAllGainTest,
	"Wyvrn.Haptics.VoicePoolStopAllReleasesTriggers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHapticVoicePoolStopAllGainTest::RunTest(const FString& Parameters)
{
	// StopAll() is what the render worker calls when haptics are switched off at the
	// master control. Silencing is not enough there: adaptive-trigger resistance is
	// deliberately independent of the vibration gain, so a latched claim would keep
	// the trigger stiff. StopAll must stop the voices AND release both triggers.
	FWyvrnRuntimeData Data;
	Data.EffectJson.Add(TEXT("{}")); // EffectId 0 -> stiffness-carrying, looping
	Data.EffectJson.Add(TEXT("{}")); // EffectId 1 -> plain vibration
	Data.Commands.Add(MakeStiffnessPlay(TEXT("Hold"), 0, -1, EWyvrnHapticSide::Global));
	Data.Commands.Add(MakePlay(TEXT("Plain"), 1, -1, EWyvrnHapticPriority::High, EWyvrnHapticMixing::Merge));

	FMockInterhapticsRuntime Runtime;
	FHapticVoicePool Pool(Runtime, 1);
	Pool.Preload(Data);

	Pool.PlayCommand(Data.Commands[0], 0.0); // Hold -> arms L2 + R2
	Pool.PlayCommand(Data.Commands[1], 0.0); // Plain -> a second live voice
	TestEqual(TEXT("both triggers armed before the gain drops"), Runtime.TriggerStarts.Num(), 2);
	TestEqual(TEXT("two voices live before the gain drops"), Pool.GetActiveVoiceCount(), 2);

	Pool.StopAll();
	TestEqual(TEXT("HAR told to stop everything"), Runtime.StoppedAllCount, 1);
	TestEqual(TEXT("no voice left playing"), Pool.GetActiveVoiceCount(), 0);
	TestEqual(TEXT("both triggers released"), Runtime.TriggerStops.Num(), 2);
	TestTrue(TEXT("L2 released"), Runtime.TriggerStops.Contains(true));
	TestTrue(TEXT("R2 released"), Runtime.TriggerStops.Contains(false));

	// Idempotent: a second call must not re-issue trigger releases, or every frame at
	// gain 0 would spam scePadSetTriggerEffect.
	Pool.StopAll();
	TestEqual(TEXT("no duplicate trigger releases"), Runtime.TriggerStops.Num(), 2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHapticVoicePoolTriggerToggleTest,
	"Wyvrn.Haptics.VoicePoolTriggerToggle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHapticVoicePoolTriggerToggleTest::RunTest(const FString& Parameters)
{
	// The adaptive triggers have their own on/off, independent of playback and of the
	// vibration gain. Disabling releases the pad but keeps the claims latched, so
	// re-enabling re-arms them; playback itself is never disturbed.
	FWyvrnRuntimeData Data;
	Data.EffectJson.Add(TEXT("{}"));
	Data.Commands.Add(MakeStiffnessPlay(TEXT("Hold"), 0, -1, EWyvrnHapticSide::Global));

	FMockInterhapticsRuntime Runtime;
	FHapticVoicePool Pool(Runtime, 1);
	Pool.Preload(Data);

	Pool.PlayCommand(Data.Commands[0], 0.0); // arms L2 + R2 with material 1
	TestEqual(TEXT("both triggers armed"), Runtime.TriggerStarts.Num(), 2);

	Pool.SetAdaptiveTriggersEnabled(false);
	TestEqual(TEXT("disabling releases both triggers"), Runtime.TriggerStops.Num(), 2);
	TestEqual(TEXT("disabling does not stop the voice"), Pool.GetActiveVoiceCount(), 1);
	TestEqual(TEXT("disabling issues no new arms"), Runtime.TriggerStarts.Num(), 2);

	// Redundant toggles must not reach the pad at all.
	Pool.SetAdaptiveTriggersEnabled(false);
	TestEqual(TEXT("re-disabling is a no-op"), Runtime.TriggerStops.Num(), 2);

	// While disabled, a newly played stiffness event claims but must not arm.
	Pool.PlayCommand(Data.Commands[0], 1.0);
	TestEqual(TEXT("no arming while disabled"), Runtime.TriggerStarts.Num(), 2);

	Pool.SetAdaptiveTriggersEnabled(true);
	TestEqual(TEXT("re-enabling re-arms from the surviving claims"), Runtime.TriggerStarts.Num(), 4);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
