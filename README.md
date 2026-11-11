# Tsunami CBF: predictive input buffer for Geometry Dash

**Game version:** GD **2.2081** with Geode **v5.11.2** (Windows + Android).
**2.208 is not supported:** no Geode release exists for 2.208, so nothing can load mods on it. Update GD to 2.2081.

Made by **Malone Ocean**.

---

## Getting the `.geode` file (no coding setup needed)

GitHub builds the mod for free.

1. Create a free account at github.com, then click **New repository**. Name it `tsunami-cbf`, set it to Public, and create it.
2. On the new repo page, click **uploading an existing file**. Drag in **everything from this folder**, including the hidden `.github` folder. (If your file browser hides it, upload `.github/workflows/build.yml` on its own.) Commit.
3. Open the **Actions** tab. The "Build Tsunami CBF" run starts by itself and takes about 5–10 minutes.
4. When it shows a green check, click the run. Under **Artifacts**, download **Tsunami CBF**, then unzip it. Inside is `maloneocean.tsunami-cbf.geode`.
5. Copy that file into your GD mods folder:
   - Windows (Steam): `...\steamapps\common\Geometry Dash\geode\mods\`
   - Or in-game: Geode → mods list → folder icon.
6. Restart GD. A cyan wave button appears in the top-left of the main menu and in the pause menu.

### Building locally instead
With the Geode CLI + SDK v5.11.2 installed: `geode build` in this folder.

---

## How it works

GD 2.2 runs physics at 240 ticks per second. When you click:

1. **Capture.** The click is caught at the tick it arrived on, and the full game state is saved (using GD's own checkpoint system plus the input state checkpoints don't cover).
2. **Predict.** The mod simulates the next *window + 30* ticks with your click applied right away. If you survive, nothing changes.
3. **Buffer.** If that click would kill you, it tries the click 1, 2, 3, … ticks later (up to the window size) and picks the earliest tick that survives. The click is held back and released on that tick. If your finger already let go, the tap length is kept.
4. **Death rewind.** If you still die inside the window (for example because you held longer than predicted), the mod rewinds to your click, replays your real inputs with the click moved to a safe tick, and carries on. This happens at most once per click.

Simulated futures are silent: sound effects and deaths inside a simulation never reach you.

### The slider
| Buffer rate | Window | Time |
|---|---|---|
| 240 TPS | 1 tick | 4.2 ms |
| 60 TPS | 4 ticks | 16.7 ms |
| 15 TPS (default) | 16 ticks | 66.7 ms |
| 4 TPS | 60 ticks | 250 ms |
| 1 TPS | 240 ticks | 1000 ms |

Use the − / + buttons for single steps.

---

## Notes
- **Don't run it with Click Between Frames or CBF Lite.** They are marked incompatible because they also take over input timing.
- Large windows (below about 10 TPS) on very heavy levels can cause a short hitch when a click has to be searched. The per-click simulation budget caps it.
- Prediction relies on GD's checkpoint system. Any trigger that practice mode restores imperfectly can also make a prediction slightly off. Death Rewind catches most of these cases.
- This is an input-assist mod. Leaderboards and demon lists treat it as a hack, so turn on **Safe Mode** if you don't want progress saved on rated levels.

## Files
- `src/Engine.cpp`: capture, simulation, delay search, death rewind
- `src/Hooks.cpp`: GD hooks (physics step, input, death, audio, menu buttons)
- `src/TsunamiPopup.cpp`: the light-blue GUI (Buffer + Credits tabs)
- `src/Config.cpp`: saved settings
