AI Tactical Battle Simulation (C++ / OpenGL)

Final project for an Artificial Intelligence course. A fully autonomous battle simulation between two teams (Team A - Red, Team B - Blue) in a maze environment. The project is written in C++ using the OpenGL (FreeGLUT) library and focuses on implementing advanced decision-making, navigation, and squad tactics algorithms.

Key Features

Full Autonomy: The characters analyze their environment, make decisions, and act completely independently without player intervention.

Modular Architecture (OOP): The codebase is structured using Object-Oriented Programming principles, divided into classes representing Agents, navigation Cells, and a central game manager.

Real-Time Combat Log: A live UI display of executed actions (shooting, throwing grenades, healing, melee combat) color-coded by team.

Post-Game Statistics Screen: At the end of the battle, an organized table displays the status of the surviving agents (role, trait, HP, and ammo).

Audio Support: Integration of sound effects (gunfire and grenade explosions) for a complete visual and auditory experience.

AI Algorithms & Mechanics

1. Finite State Machine (FSM)
Every Agent operates based on a dynamic state machine updated every turn:

STATE_HUNT: Searching for the enemy and moving between rooms.

STATE_COMBAT: Managing tactical combat inside a room, including taking cover, shooting from a distance, or engaging in melee combat when out of ammo.

STATE_FLEE: Escaping from combat to another room when the odds of survival are low.

STATE_SEEK_MEDIC / STATE_SEEK_AMMO: Strategic retreat to group up with a support unit (Medic/Supplier) or to find pickups scattered across the map.

2. A* Pathfinding
Every movement on the map, whether to the center of a room, chasing an enemy, or fleeing, is calculated using the A* (A-Star) algorithm, ensuring the shortest path while avoiding walls and obstacles.

3. Dynamic Influence Map & Line of Sight (LOS)
During combat inside a room, agents calculate a "Safety Score" for every possible adjacent cell:

Using Raycasting (Bresenham's line algorithm) to check Line of Sight (LOS) to enemies.

A cell providing Cover receives a higher safety score for soldiers with a "Cautious" trait.

Heatmap Visualization: Users can press the M key to toggle a real-time display of the danger map in the room (Red = Exposed/Danger, Green = Safe area).

4. Squad Tactics & Cohesion
Agents operate as a coordinated team rather than solitary individuals:

Vanguard Mechanic: Support units (Medics and Suppliers) do not wander randomly. They identify the "Vanguard" – the fighter currently on the front line facing the enemy – and follow them to provide close support in real-time.

Baiting / Traps: When a fighter is inside a room and their enemy is in the corridor, they will retreat to the center of the room, clearing the doorway to lure the enemy inside into a disadvantageous position (since fighting in corridors is restricted).

5. Anti-Deadlock / Forced Retreat
A classic issue in Multi-Agent Pathfinding is collisions in narrow corridors. This project includes a sophisticated deadlock-breaking mechanism:

If an agent detects it is blocked and cannot move for several consecutive turns, it enters an emergency state (isForcedRetreat).

It suspends its current mission, locates the center of the nearest empty room, and forcefully retreats there to clear the path for the rest of the team.

Entities & Roles
Each team consists of 4 agents with different roles and "traits" that significantly affect their behavior:

Fighters: Carry a weapon (30 bullets) and can throw grenades. Can have one of two traits:

Aggressive (F(A)): Will prefer to expose themselves to gain Line of Sight and eliminate the enemy quickly. Will only retreat when their health is critically low.

Cautious (F(C)): Prefer to move from cover to cover and avoid unnecessary risks.

Medic (M): Unarmed support unit. Follows the fighters and can restore their health (HP) to 100% upon contact.

Supplier (S): Unarmed support unit. Replenishes the fighters' ammunition back to 30 bullets upon contact.


How to Run
Ensure you have a C++ IDE installed (e.g., Visual Studio).

Make sure the FreeGLUT library is properly configured in your project (include glut.h, and link freeglut.lib, glew32.lib).

Ensure the audio files shoot.wav and grenade.wav are placed in the main execution directory.

Compile and run.

Controls:

M Key: Toggle the visual Influence Map (Heatmap) inside rooms.

Left Mouse Click: Click the RESTART GAME button in the bottom right panel to reset the maze and start a new battle.


Project Structure
main.cpp - The core of the system. Generates the maze, computes the A* algorithm, runs the game loop (Update), and handles OpenGL drawing functions.

Agent.h / Agent.cpp - Defines the Agent class (FSM, personal data, equipment, and traits).

Cell.h / Cell.cpp / CompareCells.h - Infrastructure for the navigation algorithm (A*).

shoot.wav / grenade.wav - Audio assets.