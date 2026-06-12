# AI Tactical Battle Simulation

![C++](https://img.shields.io/badge/C%2B%2B-17-blue?logo=cplusplus)
![OpenGL](https://img.shields.io/badge/OpenGL-FreeGLUT-green)
![Platform](https://img.shields.io/badge/Platform-Windows-lightgrey?logo=windows)

A fully autonomous battle simulation between two AI-controlled teams (Red vs. Blue) navigating a procedurally structured maze. Built in C++ with OpenGL (FreeGLUT) as a final project for an Artificial Intelligence course.

> Add a screenshot or GIF here once you have one — e.g.:  
> `![Demo](demo.gif)`

---

## Table of Contents

- [Features](#features)
- [AI Systems](#ai-systems)
- [Agent Roles](#agent-roles)
- [Prerequisites](#prerequisites)
- [Setup & Build](#setup--build)
- [Controls](#controls)
- [Project Structure](#project-structure)

---

## Features

- **Full Autonomy** — Agents analyze their environment and act independently with no player input.
- **Real-Time Combat Log** — Live UI panel showing actions (shooting, grenades, healing, melee), color-coded by team.
- **Post-Game Stats Screen** — Summary table of surviving agents showing role, trait, HP, and ammo.
- **Audio** — Sound effects for gunfire and grenade explosions via `shoot.wav` / `grenade.wav`.
- **Influence Map Toggle** — Press `M` to overlay a real-time danger heatmap (red = exposed, green = safe).

---

## AI Systems

### 1. Finite State Machine (FSM)
Each agent runs a state machine updated every tick:

| State | Behavior |
|---|---|
| `HUNT` | Roam between rooms searching for enemies |
| `COMBAT` | Engage in-room: take cover, shoot, or melee when out of ammo |
| `FLEE` | Retreat to another room when survival odds are low |
| `SEEK_MEDIC` | Fall back to the team Medic for healing |
| `SEEK_AMMO` | Fall back to the Supplier or map pickups for ammo |

### 2. A\* Pathfinding
All movement — chasing, fleeing, or regrouping — is routed via A* to find the shortest obstacle-free path.

### 3. Influence Map & Line of Sight (LOS)
During room combat, agents score every adjacent cell for safety:
- **Raycasting** (Bresenham's algorithm) determines LOS to enemies.
- Cells behind cover receive bonus safety score, especially for `Cautious` fighters.

### 4. Squad Tactics
- **Vanguard mechanic** — Medics and Suppliers identify the frontline fighter and shadow them rather than wandering.
- **Baiting / Traps** — A fighter cornered in a room retreats to its center, vacating the doorway to lure enemies into an unfavorable position.

### 5. Anti-Deadlock (Forced Retreat)
When an agent is blocked for several consecutive turns it enters `isForcedRetreat` mode: suspends its current goal, pathfinds to the nearest empty room's center, and clears the corridor for teammates.

---

## Agent Roles

Each team fields **4 agents**:

| Role | Weapon | Trait | Behavior |
|---|---|---|---|
| Fighter (Aggressive) | 30 bullets + grenades | Aggressive | Prioritizes LOS and quick kills; retreats only when critically low on HP |
| Fighter (Cautious) | 30 bullets + grenades | Cautious | Moves cover-to-cover; avoids unnecessary exposure |
| Medic | None | — | Follows the vanguard fighter; restores HP to 100% on contact |
| Supplier | None | — | Follows the vanguard fighter; restores ammo to 30 on contact |

---

## Prerequisites

- **OS:** Windows (x64)
- **IDE:** Visual Studio 2019 or later
- **Libraries (included in repo):**
  - FreeGLUT (`freeglut.lib` / `freeglut.dll`)
  - GLEW (`glew32.lib` / `glew32.dll`)
- **Audio files** (`shoot.wav`, `grenade.wav`) must be in the executable's working directory

---

## Setup & Build

1. **Clone the repository**
   ```
   git clone https://github.com/NirLevy7/AI-Battle-Simulation.git
   cd AI-Battle-Simulation
   ```

2. **Open in Visual Studio**  
   Open `AI-Project.slnx` (or `AI-Project.vcxproj`) in Visual Studio.

3. **Verify library paths**  
   The project is pre-configured to link against the included `freeglut.lib` and `glew32.lib`. If you move files, update:
   - **Include Directories** → path to `freeglut.h` / `glew.h`
   - **Library Directories** → path to `.lib` files
   - **Linker → Input** → `freeglut.lib;glew32.lib;opengl32.lib`

4. **Place DLLs alongside the executable**  
   Copy `freeglut.dll` and `glew32.dll` into `x64\Debug\` (or `Release\`) next to `AI-Project.exe`. The audio files (`shoot.wav`, `grenade.wav`) must be there too.

5. **Build and run**  
   Press `Ctrl+F5` in Visual Studio (or `F5` to debug).

---

## Controls

| Key / Action | Effect |
|---|---|
| `M` | Toggle influence map heatmap overlay inside rooms |
| Left Mouse Click on **RESTART GAME** | Reset the maze and start a new battle |

---

## Project Structure

```
AI-Battle-Simulation/
├── main.cpp          # Game loop, maze generation, A* algorithm, OpenGL rendering
├── Agent.h/.cpp      # Agent class: FSM, traits, equipment, decision-making
├── Cell.h/.cpp       # Grid cell data and navigation support for A*
├── shoot.wav         # Gunfire sound effect
├── grenade.wav       # Grenade explosion sound effect
├── freeglut.dll/.lib # FreeGLUT runtime and import library
└── glew32.dll/.lib   # GLEW runtime and import library
```
