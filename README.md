# Mech Trak

> A BakkesMod plugin for Rocket League that automatically tracks your custom training sessions and syncs your stats to an online dashboard — helping you build consistency and measure real improvement.

---

## Overview

Mech Trak is designed for players who take **custom training seriously**. The plugin runs silently in the background during your training sessions, collecting data on your attempts and goals, and automatically uploading it to the Mech Trak web dashboard every 30 seconds. No manual logging. No interruptions. Just train and review.

---

## Features

**Plugin (In-Game)**
- Auto-loads with your active session from the dashboard
- Tracks shot attempts and goals scored with custom detection logic
- Customizable HUD overlay displayed during training
- Automatically syncs data to the dashboard every 30 seconds

**Dashboard (Web)**
- Create and manage custom training sessions
- Load a session to begin tracking
- Visualize your progress across all sessions using 6 different graph types

![Stats Dashboard Screen](https://github.com/xnathangithub/MechTrak/blob/main/images/home%20%3E%20stats.png?raw=true)
*The Mech Trak Stats Page*
---

## Requirements

- [BakkesMod](https://bakkesmod.com/) (latest version recommended)
- A Mech Trak account — [sign up at the dashboard](#)
- Rocket League (PC)

---

## Installation

### 1. Download the Plugin
Sign into your [Mech Trak Dashboard](#) and download the `.dll` plugin file.

### 2. Install the Plugin
Place the `.dll` file in your BakkesMod plugins folder:
```
%appdata% > Roaming > bakkesmod > plugins
```

### 3. Load the Plugin
Start Rocket League and enter a **Custom Training** pack. If the HUD does not appear automatically:
1. Press `F6` to open the BakkesMod console
2. Type the following and press Enter:
```
plugin load mechtrak
```

### 4. Start a Session
Head to the **Mech Trak Dashboard** and create a new session. Once active, the plugin will begin tracking your attempts and goals, uploading data every 30 seconds.

### 5. Adjust the HUD (Optional)
If the HUD is too large for your screen:
1. Press `F2` to open BakkesMod settings
2. Navigate to the **Plugins** tab
3. Select **MechTrak Plugin** and adjust the HUD scale to your preference

---

## How It Works

Mech Trak uses BakkesMod's SDK hooks to detect in-game events and infer training attempts without any user input.

**Attempt Detection**

An attempt is counted on either:
- A **ball explosion** (goal scored or ball hits the ground after time expires)
- A **manual ball reset** by the player — but *only if the player has touched the ball first*, preventing accidental counts when a round hasn't started

**Goal Detection**

When a goal is scored, both an attempt and a goal are recorded simultaneously.

**Replay Correction**

Goal replays can cause false positives — the ball explosion during a replay would otherwise count as an extra attempt. To fix this, a **12-second ignore timer** starts after every goal. This timer is cancelled early if:
- The player manually resets the ball
- The player touches the ball again

This ensures legitimate attempts made immediately after a goal are still counted accurately.

---

## Known Issues

- In rare cases (typically on the very first attempt of a session), a goal may be recorded without a corresponding attempt. This is a known edge case being investigated.

---

## License

This project is an independent tool and is not affiliated with, endorsed by, or sponsored by Psyonix or Epic Games. Rocket League and related marks are trademarks of their respective owners. All plugin code is original work by the Mech Trak author.

---

## Credits

- [BakkesMod](https://bakkesmod.com/) — for the incredible modding SDK that makes this possible
- Claude (Anthropic) — AI assistance during development
