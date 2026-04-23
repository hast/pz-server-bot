# pz-server-bot

Discord bot for a Project Zomboid server. The bot currently talks to Discord through D++ and talks to the game server through Source RCON.

## Current Features

- Registers a `/ping` slash command.
- Provides a temporary `/rcon command:<command>` slash command for debugging server RCON output.
- Polls Project Zomboid RCON with the `players` command.
- Builds a status embed with current player count and player list.
- Publishes the status through a Discord webhook.
- Edits one status webhook message after the first publish instead of spamming a new message every poll.

## Current Config

`config.json` is ignored by git because it contains secrets.

Expected shape:

```json
{
  "token": "discord bot token",
  "guild_id": 123456789012345678,
  "webhook_url": "discord status webhook url",
  "rcon_host": "127.0.0.1",
  "rcon_port": 16262,
  "rcon_password": "project zomboid rcon password",
  "poll_seconds": 60
}
```

Notes:

- `guild_id` is optional. Use it during development so slash commands appear quickly in one server.
- `webhook_url` is for the status embed channel.
- `rcon_port` should be the RCON TCP port, not the game port. Usually `DefaultPort=16261` and `RCONPort=16262`.
- Rotate any Discord bot token or webhook URL that gets pasted into chat or committed by accident.

## Implemented Structure

The bot is split into:

- `src/main.cpp`: app/bootstrap and polling loop.
- `src/rcon_client.cpp`: Source RCON client.
- `src/player_status.cpp`: player parsing, embed creation, webhook publishing.
- `include/pzserverbot/*.h`: public project headers.

## Near-Term TODOs

### 1. Harden `/rcon`

`/rcon` is powerful and should not stay public.

TODO:

- Restrict `/rcon` to one Discord user id or an admin role.
- For long responses, attach a `.txt` file instead of truncating.
- Optional: add `/rconfind command:<command> query:<text>` to search huge RCON outputs like `showoptions`.

### 2. Join/Leave Log Channel

Add a second webhook for a log channel:

```json
{
  "log_webhook_url": "discord log webhook url"
}
```

Track previous online players in memory:

```text
joined = current_players - previous_players
left   = previous_players - current_players
```

On first poll, initialize state without sending messages. On later polls, send:

```text
[15:26] Bob joined the game
[15:46] Bob left the game
```

This feature should be done before the database because the database session logic will reuse the same join/leave detection.

### 3. Improve Status Embed

The status embed should eventually show:

- Current player count.
- Current player list.
- Last updated time.
- RCON/server error state.
- Optional server stats from RCON:
  - `stats performance all`
  - `stats game all`
  - memory usage
  - FPS
  - zombie/player counters

### 4. Persist Status Message ID

Current behavior:

- The bot creates one webhook status message per bot process.
- It edits that message while running.
- If the bot restarts, it creates a new status message.

TODO:

- Store the Discord webhook message id in `state.json`, `config.json`, or SQLite later.
- On startup, load that id and edit the existing status message.

There is already a TODO in `src/main.cpp` for this.

## Later Features

### Player Activity Database

Use SQLite for tracking history.

Possible tables:

```text
players
- id
- name
- first_seen_at
- last_seen_at

player_sessions
- id
- player_id
- login_at
- logout_at nullable
- last_seen_at

player_snapshots
- id
- checked_at
- online_count
- raw_response optional

player_snapshot_members
- snapshot_id
- player_id
```

This enables:

- Total playtime per player.
- First seen / last seen.
- Session history.
- Online count over time.

### Graphs and Reports

Use the database to generate:

- Player count over time.
- Peak player count by day.
- Total hours played per player.
- Daily/weekly activity.
- Average session length.

This can start as generated images or text reports, then grow into a web dashboard later.

## In-Game Clock / Telemetry Mod

Vanilla Project Zomboid RCON does not appear to expose current in-game date/time.

Checked commands:

- `help`
- `showoptions`
- `stats list`
- `stats game all`
- `stats performance all`

Useful stats exist, but no live world date/time was found.

### Implemented Mod

Use the companion Project Zomboid server mod:

```text
Workshop ID: 3712575846
Mod ID: ServerTimeJson
```

The mod loads the actual server time/date and writes it to:

```text
mod/3712575846/mods/ServerTimeJson/common/server_time.json
```

The Lua writer lives at:

```text
mod/3712575846/mods/ServerTimeJson/42/media/lua/server/ServerTimeJson.lua
```

It uses `getGameTime()` and writes at most once every 10 seconds.

This is the preferred source for the bot's in-game clock because it reads real server state instead of estimating time from `DayLength`.

Current telemetry format:

```json
{
  "year": 1993,
  "month": 7,
  "day": 14,
  "hour": 3,
  "minute": 10,
  "date": "1993-07-14",
  "time": "03:10:00"
}
```

Field notes:

- `month` is 1-based because the Lua mod converts `getMonth() + 1`.
- `day` comes from `getDayPlusOne()`.
- `hour` and `minute` are derived from `getTimeOfDay()`.
- `date` and `time` are already formatted for display.

Set `telemetry_path` in `config.json` to point at the generated `server_time.json` file.

Why file bridge:

- Simple.
- Debuggable.
- No HTTP server needed in the bot.
- Handles world pause correctly because `ServerTimeJson` writes actual server time, not an estimate.

Do not edit Discord every second for a live timer. The mod writes JSON every 10 seconds, and the bot can read that file when it updates the Discord embed every 30-60 seconds.

## Possible Architecture Later

```text
Project Zomboid Server
  -> RCON players/stats
  -> telemetry JSON from server mod

pz-server-bot
  -> reads RCON players
  -> reads telemetry JSON
  -> updates static Discord status embed
  -> sends join/leave log webhook events
  -> stores player sessions in SQLite
  -> generates reports/graphs
```

## Useful RCON Commands

```text
players
help
showoptions
stats list
stats game all
stats performance all
stats connection all
stats network all
```

## Build

From WSL:

```bash
cmake --build build
```
opt/pzserver/steamapps/workshop/content/108600/3712575846/mods/ServerTimeJson/common/server_time.json