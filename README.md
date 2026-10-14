# Controller Workshop

A controller tester and per-game button remapper for modded (RGH/JTAG) Xbox 360 consoles.

Test every button, stick and trigger on a live picture of the controller, check sticks for drift
and worn-out spots, try the rumble motors and see the battery level. Then remap buttons for any
game. A small DashLaunch plugin applies your layout every time that game starts.

![Controller Workshop, dark theme](docs/tester-dark.png)

> The screenshots in this README are rendered by the PC preview tool in `tools/preview`, which
> runs the app's own drawing code. They look the same on the console.

## Features

**Controller tester**
- Live controller picture: buttons, D-pad, bumpers and stick clicks light up; sticks move;
  triggers fill.
- Button check: all 16 inputs turn green once they've worked, with a press count. A button that
  never turns green is broken.
- Stick panels: raw values (-32768..32767) and percentages, angle, a motion trail, and an outline
  of the furthest point reached in 64 directions, which shows worn sticks that can't reach the edge.
- Trigger bars with the highest value reached, so you can spot a trigger that never gets to 255.
- Drift test: put the controller down and it measures how far each stick and trigger rests from
  center, then rates them GOOD / SLIGHT / DRIFT (sticks) and GOOD / STUCK (triggers).
- Rumble test: drive each motor by hand with the triggers, or run 1-second and pulse patterns.
- Battery level for wireless controllers, up to 4 controllers, and the Guide button's ring of
  light shows which port you're looking at.

**Button remapper**
- Per-game profiles plus an "All games" default.
- Map any button or trigger to any other, or turn it off. Swap the sticks, or invert either
  stick's up/down.
- Try a layout in the app before saving it.
- Applies automatically when the game starts. No need to open the app again.

**Light and dark themes**

| Dark | Light |
|---|---|
| ![Dark theme](docs/tester-dark.png) | ![Light theme](docs/tester-light.png) |

## Requirements

- An RGH or JTAG Xbox 360 that runs unsigned code (dev-kit-signed XEXs, as built by the XDK).
- **DashLaunch** for the button remapper (the plugin loads through it). The tester works without it.
- To build from source: Visual Studio 2010 and the Xbox 360 XDK (tested with 21256). Neither is
  included in this repository.

## Install

1. Download `default.xex` and `ControllerWorkshopPlugin.xex` from the
   [Releases](../../releases) page and copy both to **`Hdd:\ControllerWorkshop\`** on the console.
   - `default.xex` is the app.
   - `ControllerWorkshopPlugin.xex` is the remap plugin.

   The app itself can live anywhere, but its data always goes to `Hdd:\ControllerWorkshop\`.
2. For the remapper, add the plugin to DashLaunch's `launch.ini` under `[Plugins]`, in a free
   slot (`plugin1` to `plugin5`), then restart the console:
   ```ini
   [Plugins]
   plugin4 = Hdd:\ControllerWorkshop\ControllerWorkshopPlugin.xex
   ```
3. Start `default.xex` from Aurora, FSD, or XeXMenu. Add it to your Aurora scan paths
   if you want it in your app list.

### Uninstall
Remove the plugin line from `launch.ini`, restart, and delete `Hdd:\ControllerWorkshop\`.

## Using the tester

The app opens on the tester. Every button you press is tested right away; there's nothing to start.

| Screen part | What it shows |
|---|---|
| Controller picture | Buttons, D-pad, bumpers and stick clicks light up while held. Sticks move, triggers fill. |
| Stick panels | Raw X/Y and %, magnitude and angle, a fading trail, the furthest point reached in each of 64 directions (blue outline) and the last drift test result. |
| Trigger bars | Live value 0-255, the threshold where games count it as pressed (30, orange mark) and the highest value reached (blue mark). |
| Button check | All 16 inputs. Outlined green once they've worked; the number is the press count. |
| Top right | Battery, connected ports (1-4) and the controller type. **Auto** follows whichever controller was used last. |

**Hold BACK for one second** to open the menu (D-pad to move, A to select, B to close).
Every other button is part of the test, so BACK is the only one that opens the menu.

| Menu item | What it does |
|---|---|
| Resume | Back to the tester. |
| Controller | **Auto**, or lock to one connected port. Only ports with a controller are offered. |
| Rumble test | LT and RT drive the left (heavy) and right (light) motors. A = both for 1 s, X = left, Y = right, RB = alternating pulse, B = back. |
| Drift test | Let go of the controller. After a 2-second countdown it measures for 2 seconds and shows the results. |
| Button remapper | See below. |
| Theme | Dark or Light (left/right or A). Remembered between sessions. |
| Reset stats | Clears the button check, press counts, reach outline and drift results for the shown controller. |
| Exit to dashboard | Goes back to the dashboard. |

### Reading the results

- **Stick drift**: run the drift test. Under 10% is normal, 10-20% is slight wear, over 20% will
  show up in games as movement you didn't make. You can also watch the dot: it should come back to
  the middle when you let go.
- **Worn sticks**: spin each stick slowly around its edge. The blue outline should be a nearly full
  circle. A dent in one direction (a low "lo" %) means the stick can't reach the edge there.
- **Dead buttons**: press everything once. Anything still gray in the button check didn't register.
- **Triggers**: pull each one all the way. "Full range OK" means it reaches 255. A trigger resting
  above 30 in the drift test is reported as STUCK, because games treat it as held.
- **Battery**: shown as Empty / Low / Medium / Full, the same steps as the Guide's battery icon.
  The console only reports these steps, not a percentage. Wired controllers show WIRED.

![Drift test results](docs/drift-results.png)

## Using the button remapper

Open **Button remapper** from the menu and pick a game, or **All games** to set a default for
every game without its own profile. Games with a profile are marked CUSTOM.

![Game list](docs/remap-games.png)

In the editor, the left column is the button you press and the right column is what the game
receives. Changed rows are orange.

| Button | Action |
|---|---|
| D-pad up/down | Choose a row |
| D-pad left/right | Change what the button does (any button, LT/RT, or NONE to turn it off), or switch an option on/off |
| Y | Try the layout: every button you press shows what the game will see. Hold BACK for 1 second to stop. |
| X | Reset to the normal layout |
| START | Save |
| B | Back (asks first if you have unsaved changes) |

![Remap editor](docs/remap-editor.png)

Good to know:
- The layout is applied by the plugin every time the game starts. Changes take effect the next
  time you start the game.
- A game's own profile takes priority over **All games**.
- **All games** never applies to the dashboard, Aurora, FSD, XeXMenu or Controller Workshop itself,
  so a bad default can't lock you out of your menus.
- A profile applies to every controller connected while the game runs.
- A trigger mapped to a button counts as pressed past the normal threshold (30); a button mapped to
  a trigger presses it fully. Trigger-to-trigger keeps the analog value.
- Saving the normal layout deletes the profile, so the plugin leaves that game alone.
- The Guide button can't be remapped.

**Which games are listed:** every game started while the plugin was running, plus games installed on the hard drive (Games on Demand, Arcade and disc
installs). A game you haven't started since installing the plugin may not be listed yet; start it
once and it will appear.

## Files on the console

Everything lives in `Hdd:\ControllerWorkshop\`:

| File | Contents |
|---|---|
| `default.xex` | The app |
| `ControllerWorkshopPlugin.xex` | The DashLaunch plugin |
| `Remap\<TITLEID>.ini` | One remap profile per game (`ALL.ini` = All games) |
| `titles.txt` | Games the plugin has seen (`TITLEID\|Name`), used for the game list |
| `settings.ini` | App settings (`theme=dark` or `theme=light`) |
| `plugin.log` | What the plugin did for each game; useful when something doesn't work |

Profiles are plain text and can be edited or shared:

```ini
; Controller Workshop remap profile
name=Halo 3
A=B
B=A
LT=RT
RT=LT
BACK=NONE
swap_sticks=0
invert_ly=0
invert_ry=1
```

Button names: `A B X Y LB RB LS RS BACK START UP DOWN LEFT RIGHT LT RT`, or `NONE`. Buttons left
out keep their normal function.

## Troubleshooting

- **The remapper says "Plugin not running".** Check the `launch.ini` line points to
  `Hdd:\ControllerWorkshop\ControllerWorkshopPlugin.xex` and that you restarted the console. If
  `Hdd:\ControllerWorkshop\plugin.log` doesn't exist, the plugin didn't load.
- **A remap doesn't work in a game.** Look in `plugin.log`. For each game it logs the title ID, the
  name, which profile it used, and either "remap active" or why it gave up. The plugin changes
  input that games read through the standard XInput function. A game that reads the controller
  another way won't be affected.
- **The plugin conflicts with another plugin.** Controller Workshop's plugin loads at memory
  address 0x91E80000-0x91EB1000. If another plugin uses that range, one of them won't load.

## Building from source

You need Visual Studio 2010 and the Xbox 360 XDK installed (default paths), plus Python 3 for the
plugin build. The XDK is not included and isn't needed to use the release files.
Builds go to `dist\ControllerWorkshop\` (not part of the repository).

| What | Command | Output |
|---|---|---|
| App | `build\build.bat` (or `build\build.bat Debug`) | `dist\ControllerWorkshop\default.xex` |
| Plugin | `plugin\build\build_plugin.bat` | `dist\ControllerWorkshop\ControllerWorkshopPlugin.xex` |
| Layout preview (PC) | `tools\preview\build_preview.bat` | `tools\preview\out\*.png` |

How the plugin is built: the DLL is linked at 0x98000000 with 4 KB section alignment,
`tools\pe_4k_align.py` marks it 4 KB-aligned, and `imagexex` turns it into a system DLL loaded at
0x91E80000 (`plugin\build\xex.xml`). The XEX is left exactly as imagexex writes it (dev-kit
encrypted). Modifying it afterwards breaks the header hash, and the console then won't load it.

The preview tool compiles the app's drawing code for Windows against a stub `xtl.h`, draws each
screen to SVG with scripted controller input, and converts the SVGs to PNG with headless Microsoft
Edge. It's the quickest way to check a layout change without a console.

### Project layout

```
src/            the app
  main.cpp        tester screens, menu, drift and rumble tests
  remap_ui.cpp    remapper screens
  remap.cpp       profile format and remapping (shared with the plugin)
  games.cpp       game list (titles.txt, installed content, existing profiles)
  render.cpp      2D renderer (D3D9, no textures) and text with the built-in font
  shape.cpp       controller outline (spline + triangulation)
  theme.cpp       dark and light colors, settings.ini
  battery.cpp     battery level from xam
  system.cpp      hard drive access, plugin detection, migration from the old name
  font8x8.h       8x8 bitmap font
plugin/         the DashLaunch plugin (title detection, game names, input hook, log)
build/          app project and build script
tools/          pe_4k_align.py, trace_outline.py (traces a controller outline from a picture), preview/
docs/           README screenshots
```

## Known limitations

- The Guide button can't be tested or remapped; the system keeps it for itself.
- Battery level comes in four steps (Empty / Low / Medium / Full); there's no percentage.
- A remap profile applies to all connected controllers, not per controller.
- Remap changes apply the next time the game starts.

## Credits

- 8x8 font: [font8x8](https://github.com/dhepper/font8x8) by Daniel Hepper (public domain),
  based on the IBM PC BIOS font.
- Xbox 360 internals (xam ordinals, SPA/XDBF layout, loader structures) from the
  [Xenia](https://github.com/xenia-project/xenia) project's research, and the device-stats layout
  from the community `xkelib` headers.

## License

[MIT](LICENSE).
