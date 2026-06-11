# CS 1.6 E-BOT FIXED ![GitHub all releases](https://img.shields.io/github/downloads/EfeDursun125/CS-EBOT/total)
AI Bot for Counter-Strike based on SyPB, this bot is only for zombie plague/escape and biohazard gamemodes.

## FIXED version

This fork is a Windows-focused Zombie Plague 4.3 fixed build. Main changes:

- Fixed human bots selecting primary weapons after spawn instead of running with knives.
- Improved human camp selection: bots prefer reachable, nearby, less occupied camp groups.
- Added human high-spot waypoint tagging for reachable jump/crouch-jump elevated positions.
- Improved zombie chasing so active pursuit is not overwritten by patrol/fallback waypoint goals.
- Added reachable-frontier fallback when the requested goal is separated by a broken waypoint graph.
- Improved waypoint analysis for tight/crouch passages by reducing duplicate merge range and allowing point-hull crouch fallback.
- Improved zombie grenade usage and fixed grenade throw state so zombies actually throw HE/custom grenade items.
- Added ladder normalization: ladders use crouch support and do not keep jump flags.
- Added teammate semiclip-friendly behavior: old teammate blocking/avoidance is disabled by default.
- Added more natural human camp idling: bots can move inside the same camp mesh and occasionally shift aim when no enemies are nearby.
- Added server browser query hook, optional bot name tag hiding, and fake ping support.
- Added safer Windows x86 ClangCL build settings and post-build guard flag cleanup for GoldSrc compatibility.

[Click HERE To Join E-BOT Discord Community](http://discord.gg/v7PesBamXt)

[Click HERE To Join E-BOT Steam Community](https://steamcommunity.com/groups/E125G)

[Please Download From the Blog to Support This Project](https://ebots-for-cs.blogspot.com/)

# E-Bot Requires VCRedist (For Windows)
If you see badf load on console (when you type "meta list") install this.

[Click HERE To Download](https://aka.ms/vs/17/release/vc_redist.x86.exe)

# How to install
1. Download & install metamod if you dont have.
2. Download latest ebot release.
3. Put ebot folder to "cstrike\addons"
4. Open "cstrike\addons\metamod\plugins.ini"
5. (For Windows) Add that line "win32 addons\ebot\dlls\ebot.dll" and save it.
6. (For Linux) Add that line "linux addons/ebot/dlls/ebot.so" and save it.
