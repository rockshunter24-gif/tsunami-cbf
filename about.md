# Tsunami CBF

A **predictive input buffer** for Geometry Dash.

Click too early and Tsunami CBF catches it. It records the exact tick you clicked, simulates the next few ticks ahead, and if that click would kill you it holds it back. The click is then released on the first tick that survives. This works for cube, ship, ball, UFO, wave, robot, spider and swing, at any size, with orbs, pads, portals and dual mode.

## Settings (open from the main menu or the pause menu)
- **Buffer Rate**: from **240 TPS** (a 1-tick window, the tightest) down to **1 TPS** (a 240-tick window, one full second).
- **Predict Clicks**: simulates ahead the moment you click.
- **Death Rewind**: if you still die inside the window, rewinds to your click and retries it on a later tick.
- **Show Indicator**: briefly shows how many ticks a click was moved.
- **Safe Mode**: doesn't save progress on rated levels.

Made by **Malone Ocean**.
