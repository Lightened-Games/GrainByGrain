# FactoryQA: drop-in C2 (solid world) and C3 (gait) checks

Four files and one folder. Copy `FactoryQA/` into `Source/<Module>/`, rebuild, and run with a flag. You don't need to change any game code: the subsystem only exists when a flag is on the command line. Built 2026-11-07 and tested on copies of ShinyThings, PlazaDuty and WrongNumber (all Rocketbox `Bip01-*` rigs). It auto-detects bone names for UE mannequin, Rocketbox (space or dash), Bip001, Mixamo and Quaternius rigs.

| Flag | What it does |
|---|---|
| `-FactoryQA` | everything below |
| `-CollisionAudit` | static scan 5 s in: visible, reachable static meshes that don't block Pawn, have no simple collision, or don't affect navigation |
| `-PenetrationLog` | runtime: characters inside blocking geometry, overlapping each other, or standing inside a non-solid prop found by the audit |
| `-GaitLog` | per-character numbers for idle, walk and run bands |
| `-GaitShots` | front, side and back leg shots: idle, plus four stride phases of walk and run, for the player and up to `MaxShotCharacters - 1` NPCs |

Run it headless with no window (it never disturbs a playtest), using the game's own autoplay and exit flags:

```
UnrealEditor.exe "<game>.uproject" [map] -game -autoplay <game exit flag> -RenderOffscreen -FactoryQA -unattended -nosplash -nosound -abslog="<log>"
```

Never use `-nullrhi` for shots. Output goes to `Saved/QA/FactoryQA/<stamp>/report.json`, `report.txt` and `leg_<actor>_<band>_<view>.png`. Log lines start with `[FactoryQA]`. The report is rewritten every 15 s, so a killed run still leaves data. The per-band log lines are only printed at a clean exit; read `report.json` otherwise.

## Measures

- **Toe angle (foot progression).** Yaw of the foot→toe vector, measured in stance only and only when the foot is flat enough. While moving, it is relative to the direction of travel; standing, it is relative to the body (hip line). Positive means toe-in.
- **Knee.** How far the knee sits out of the leg's own plane (hip, ankle, foot direction), measured through the whole stride. Positive means bowed outward. The front-view offset is also reported (`kneeLatP95`).
- **Step width.** Distance between the ankles across the travel line. Scissor or catwalk steps show up here.
- **Slide** of a planted foot, and **contact** (toe height vs the standing baseline).
- **Bind-pose toe yaw** is logged per character, for reference only.

## Every option is a config property (`[/Script/<Module>.FactoryQASubsystem]` in DefaultGame.ini, or the probe's UPROPERTYs)

The shot options are `ShotLightMode`, `bShotIsolateSubject`, `ShotExposureBias` and `MaxShotCharacters`:
- the shot defaults are unlit, isolated (character + floor only), manual exposure, and no fog, bloom or particles;
- the default subjects are the player plus 3 NPCs.

The audit options are `NoCollideTag`, `FlatThicknessCm`, `ReachHeightCm` and `bSkipTriggerOwnerVisuals`. Translucent meshes with no collision count as effects and are skipped.

Gait thresholds: **provisional, not confirmed by the user.** See "Calibration" below and design guide C3.

## Calibration (2026-11-07, p95 unless noted)

| | PlazaDuty guard (the user's "good movement" reference) | WrongNumber crowd (UAL clips retargeted onto Rocketbox) | ShinyThings player |
|---|---|---|---|
| idle toe-out L / R | 10.7 / 21.2 (p50) | 10.7 / 21.2 (p50) | 10.1 / 21.8 (p50) |
| walk toe-out | n/a (guard only runs) | 14 / 9.5 | 12 / 10 |
| run toe-out | 7.7 / 57 (outliers) | 6-11 | 7 / 7 |
| knee out of leg plane, run | 7.5 / 3.3 | 6.4-7.6 / 1.6-2.9 | n/a |
| knee-in, walk | n/a | 3.8-6.3 | n/a |
| step width walk / run (p05 / p50) | run 12 / 15 cm | 14 / 15 cm, 12-13 / 16-18 cm | n/a |
| slide | 0.0-0.1 cm | 0.3-1.3 cm | 1.1-1.6 cm |

**Reading:** the reference measures outside the first C3 limits (knee ≤ 3 cm, toe-out ≤ 15°). Either the limits are too tight or the reference has the same flaw. The user decides. The numbers alone don't separate "good" from "bad" yet, so the shots must be judged as well. The idle toe-out is the same clip on every avatar (asymmetric 11° / 21°). The rig bind pose itself stands 14-20° turned out.

## Findings it produced

- **ShinyThings:** desk chairs, cushions and trash cans have a `NoCollision` profile, and the player walked through a chair (t = 6.8 s). Pickup gold bars and chevrons are walk-through visuals with no trigger component; tag them `NoCollide` if that is by design.
- **PlazaDuty:** 3 floating chevrons don't block (tag them). 10 cones, benches, planters and bins don't affect navigation. The guard was inside `StaticMeshActor_633/634` at Z 123-146 (t = 5.5 s).
- **WrongNumber:** collision audit PASS (361 props) and no penetrations. Walk knee-in reaches 6 cm on the right leg and is visible as a slight knock-knee in front view. The "06" number card stands off the chest.

## Glitch log (`FactoryGlitchMonitor`, added 2026-11-09)

It runs with `-FactoryQA` or `-GlitchLog` and writes `glitch.json` / `glitch.txt` next to `report.json`. Each finding gets a **close-up capture** (`glitch_<kind>_<n>.png`).

| Check | Fails when |
|---|---|
| `hitch` | real frame time over 50 ms; QA's own captures and shader compiles are ignored; fails above 3 |
| `ref-pose` | a visible character mesh is in its bind pose (T/A-pose) |
| `floating` | a shadow-casting prop has nothing under it and touches no mesh; world markers are skipped |
| `jitter` | a physics body stays awake at a crawl for 3 s |
| `camera-clip` | the player camera is inside blocking geometry |
| `stuck` | a character accelerates without moving for 2.5 s |
| `spin` | a character turns over 540°/s for 1 s |
| `log` | any Error line after warm-up (warnings are listed, not failed) |

`support-gap` is a warning: a prop visibly rests on a mesh whose collision is more than 3 cm away.

Calibration on 2026-11-09, after the fixes below:

| Game | Result | Findings |
|---|---|---|
| PlazaDuty | PASS | none |
| ShinyThings | PASS | 4 support-gap warnings: kitchen counter collision 6 cm low; a silver tray on a desk with no collision |
| WrongNumber | **FAIL** | the caught player snaps to a **T-pose** at defeat. The close-up shows it clearly, so it's a real glitch for the WrongNumber fix pass |

Fixes made during calibration:
- 300-780 ms "hitches" were the tool's own captures;
- chevrons and markers were flagged as floating;
- placards on bunk rails were flagged as floating;
- screenshots in the same frame overwrote each other;
- the monitor stopped ticking while the game was paused.

## Companion editor script

`../Editor/mesh_collision_audit.py` lists project static meshes with no simple collision (`FIX=1` adds a box). Pass the script path with forward slashes: `-script="C:/.../mesh_collision_audit.py"`, because a `\r` in a backslash path is read as an escape.
