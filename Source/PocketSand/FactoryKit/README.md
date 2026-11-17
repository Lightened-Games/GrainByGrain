# FactoryKit: start screen, cinematic shots, take-over autoplay, themed menu and legend (design guide 2.5)

A drop-in C++ folder, built and tested on 2026-11-09 in a scratch copy of Shiny Things. In that test, a full autoplay run went through every step without manual input:

1. start screen;
2. the bot pressed PLAY;
3. intro fly-in;
4. countdown;
5. play;
6. take-over, then the bot resumed after 5 s;
7. victory camera.

Every screenshot was reviewed and fixed (C5). No game names appear in the code; each game themes it entirely through config.

## Files

| File | What it does |
|---|---|
| `FactoryKitSettings.h/.cpp` | Every value: title, tagline, motif, font, colours, shadow, sizes, motion, menu texts, sounds, volume mix, flow timings, shot lists, legend rows. Edit in `DefaultGame.ini` under `[/Script/<Module>.FactoryKitSettings]` or in Project Settings > Game > Factory Kit. |
| `FactoryFlowSubsystem.h/.cpp` | The flow: Menu → Intro → (your countdown + play) → Ending. Also: take-over autoplay (a Slate input pre-processor sees all real input), options load/save/apply, legend pulses, and the QA flags. |
| `FactoryMenuWidget.h/.cpp` | The start screen. Title, accent motif line (or a texture), tagline, then PLAY / OPTIONS / QUIT. The options are master/music/SFX volume, mouse sensitivity, invert look, quality and display, with values in an aligned column. A soft left gradient scrim, text shadows, no boxes, and a 3 px accent bar plus a 12 px slide on the selection, with 200 ms ease-out. Keyboard, mouse and D-pad. |
| `FactoryCinematicCamera.h/.cpp` | Plays `FFactoryShot` lists relative to an anchor (the player, or an actor tag), eased, with a sphere sweep that keeps it out of walls. |
| `FactoryLegendWidget.h/.cpp` | Bottom-left key legend: key + label, soft shadow, no boxes, and a row that lights up about 0.3 s on its action or key. It is the visible input in the video. Also shows the bottom-right "autoplay · any key to take over" hint, which fades to 30 % after 3 s. |
| `FactoryKitStyle.h` | Shared font, shadow, sized brushes and the ease-out helper. |

## Install (about 10 minutes)

1. Copy this folder to `Source/<Module>/FactoryKit/`.
2. In `<Module>.Build.cs`, add `"UMG", "Slate", "SlateCore", "DeveloperSettings"` (plus `"AudioMixer"` if you also use FactoryRecord).
3. Add the glue to the game mode. This is everything the kit needs:

```cpp
#include "FactoryKit/FactoryFlowSubsystem.h"
// first Tick (the pawn exists), instead of starting the countdown directly:
if (UFactoryFlowSubsystem* Flow = UFactoryFlowSubsystem::Get(this))
{
    SetGameHUDVisible(false);                                   // hide the game HUD under the menu and intro
    Flow->OnStartGame.AddDynamic(this, &AMyGameMode::HandleFlowStart);  // HandleFlowStart: show the HUD, start the countdown
    Flow->Begin(bAutoplayActive);
}
// match end:      Flow->PlayEnding(bVictory, PlayerPawnOrCause);
// restart [R]:    Flow->MarkRestart(); then reload the level      (the next load skips the menu and intro)
// Esc in play:    Flow->ReturnToMenu();
// the bot:        act only when Flow->IsBotDriving()             (take-over)
// action handler: Flow->PulseLegend(TEXT("Jump"));                (player and bot paths, so the video shows inputs)
// look input:     UFactoryFlowSubsystem::GetMouseSensitivity(), GetInvertY()
```

4. Theme it in `DefaultGame.ini`:

```ini
[/Script/<Module>.FactoryKitSettings]
Title=NSLOCTEXT("FactoryKit", "T", "TETHER POINT")
Tagline=NSLOCTEXT("FactoryKit", "G", "SIXTY SECONDS OF AIR")
FontObject=/Game/<Name>/UI/Fonts/F_Display.F_Display
AccentColor=(R=1.0,G=0.62,B=0.22,A=1.0)
MotifTexture=/Game/<Name>/UI/T_OrbitMotif.T_OrbitMotif
MotifSize=(X=420,Y=60)
+LegendRows=(KeyText=NSLOCTEXT("K","W","WASD"),Label=NSLOCTEXT("K","WL","Move"),ActionId="Move",Keys=("W","A","S","D"))
```

Tune the shots per level (`MenuShots`, `IntroShots`, `VictoryShots`, `DefeatShots`; offsets are from the actor origin, which is a character's capsule centre, about 90 cm up). Set `HoverSound` / `SelectSound`, and `VolumeMix` + `MasterClass` / `MusicClass` / `SfxClass` if the game routes its sounds through classes (otherwise Master uses the device volume and Music/SFX are hidden).

## Menu layouts (no two recent games share one; design guide 2.6)

Set `MenuLayout` to one of the five options below. The kit shifts the menu shots by `MenuFrameShift` (default 70 cm) so the hero sits opposite the text. All five were tested on 2026-11-09.

| Layout | Text position | Hero position |
|---|---|---|
| `LeftColumn` | vertically centred on the left | right |
| `LowerLeft` | stacked lower left | right |
| `RightColumn` | right-aligned | left; the key hint moves to the lower left |
| `CenterBottom` | centred, low | lower right |
| `CenterStack` | centred | lower right |

The centred layouts look best with wide establishing `MenuShots` that show the environment rather than a close-up hero. Tilting the shot to fit text below the hero cropped the head, and a larger sideways shift pushed the hero out of frame.

## QA flags

- `-FactoryKitShots=1.5,6,12`: screenshots with UI at those seconds, saved to `Saved/Screenshots/<Platform>/FactoryKit_NN_<State>.png`.
- `-FactoryKitShowOptions`: opens the options page (for its screenshot).
- `-FactoryKitFakeInputAt=20`: simulates a take-over at 20 s; the log shows `Take-over: player has control`, then `bot resumed`.
- `-FactoryNoMenu`: skips the menu and intro (fast QA runs).

The log lines start with `[FactoryKit]`.

## Defaults and what the test review changed

- **Menu:** a close three-quarter sweep on the hero, then a wide one. The hero sits right of the menu column.
- **Intro:** 3.5 s from a high front three-quarter to behind the shoulder, then a 1 s blend into the gameplay camera. The first default (15 m back and high) was pulled through walls indoors and framed an empty floor.
- **Victory and defeat:** the hero sits in the right third, clear of a centred end panel. The look-at is 60 cm to the hero's right; at 130 cm the hero left the frame.
- **Menu lines:** `UImage` ignores `SetDesiredSizeOverride` without a brush. The motif and selection bar drew as 32 px squares until they got a sized `FSlateBrush`.
- **Options:** the label column had to be widened (`MenuSize × 20`) so "MOUSE SENSITIVITY" clears its value.
