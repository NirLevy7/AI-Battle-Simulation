#pragma comment(lib, "freeglut.lib")
#pragma comment(lib, "glew32.lib")
#pragma comment(lib, "winmm.lib") 

#include "glut.h"
#include <time.h>
#include <vector>
#include <queue>
#include <iostream>
#include <string>
#include <cmath>
#include <windows.h>
#include <mmsystem.h>
#include "Cell.h"
#include "Agent.h" 

using namespace std;


const int MSZ = 100;
const int UI_WIDTH = 30;
const int WALL = 1;
const int SPACE = 0;
const int ROOM_FLOOR = 2;
const int COVER = 3;
const int HEALTH_PICKUP = 4;
const int AMMO_PICKUP = 5;

int windowWidth = 1200;
int windowHeight = 900;


const double BTN_X1 = MSZ + 2.0;
const double BTN_X2 = MSZ + 28.0;
const double BTN_Y1 = 5.0;
const double BTN_Y2 = 12.0;

int maze[MSZ][MSZ] = { 0 };
int roomMap[MSZ][MSZ] = { 0 };
bool showInfluenceMap = false;

struct Room {
	int id, r, c, h, w;
};
vector<Room> rooms;
vector<Agent*> agents;
int agentIdCounter = 0;

bool gameOver = false;
Team winningTeam;
int extraDelay = 0;
int turnsWithoutDamage = 0;
bool isLastStand = false;

struct CombatEffect {
	Point start, end;
	bool isGrenade;
};
vector<CombatEffect> currentEffects;


struct LogMessage {
	string text;
	Team team;
};
vector<LogMessage> combatLog;

void static AddLog(string msg, Team t) {
	combatLog.push_back({ msg, t });
	if (combatLog.size() > 6) {
		combatLog.erase(combatLog.begin());
	}
}


struct CompareCells {
	bool operator()(const Cell* a, const Cell* b) const { return a->getF() > b->getF(); }
};

double static ManhattanDistance(int r1, int c1, int r2, int c2) {
	return abs(r1 - r2) + abs(c1 - c2);
}

bool static IsValid(int r, int c) {
	return r >= 0 && r < MSZ && c >= 0 && c < MSZ && maze[r][c] != WALL && maze[r][c] != COVER;
}

bool static IsOccupiedByAgent(int r, int c, int myId) {
	for (const auto* a : agents) {
		if (a->hp > 0 && a->id != myId && a->pos.r == r && a->pos.c == c) return true;
	}
	return false;
}

bool static HasLineOfSight(int r1, int c1, int r2, int c2) {
	int r = r1, c = c1;
	int dr = abs(r2 - r1), dc = abs(c2 - c1);
	int sr = r1 < r2 ? 1 : -1;
	int sc = c1 < c2 ? 1 : -1;
	int err = (dr > dc ? dr : -dc) / 2, e2;

	while (true) {
		if (maze[r][c] == WALL || maze[r][c] == COVER) return false;
		if (r == r2 && c == c2) break;
		e2 = err;
		if (e2 > -dr) { err -= dc; r += sr; }
		if (e2 < dc) { err += dr; c += sc; }
	}
	return true;
}

// חישוב מפת בטיחות עבור תצוגה חזותית (Heatmap)
double static CalculateCellDanger(int r, int c, Team myTeam) {
	double danger = 0;
	for (auto* a : agents) {
		if (a->hp > 0 && a->team != myTeam && a->role == FIGHTER) {
			if (HasLineOfSight(r, c, a->pos.r, a->pos.c)) {
				danger += 2.0 / (ManhattanDistance(r, c, a->pos.r, a->pos.c) + 1);
			}
		}
	}
	return danger;
}

// הניקוד לשימוש הבינה המלאכותית
double static GetSafetyScore(int r, int c, Agent& me, Agent& enemy, bool isCurrentPos) {
	double score = 0;
	double distToEnemy = ManhattanDistance(r, c, enemy.pos.r, enemy.pos.c);

	if (me.role == FIGHTER && me.ammo == 0) {
		score -= distToEnemy * 50.0;
		return score + ((rand() % 100) / 100.0);
	}

	bool hasLOS = HasLineOfSight(r, c, enemy.pos.r, enemy.pos.c);

	if (distToEnemy > 10) score -= (distToEnemy * 5.0);
	else if (distToEnemy < 3) score -= 15.0;
	else score += 10.0;

	bool hasCover = false;
	int dr[4] = { -1, 1, 0, 0 }; int dc[4] = { 0, 0, -1, 1 };
	for (int i = 0; i < 4; i++) {
		int coverR = r + dr[i]; int coverC = c + dc[i];
		if (coverR >= 0 && coverR < MSZ && coverC >= 0 && coverC < MSZ) {
			if (maze[coverR][coverC] == COVER) {
				if (ManhattanDistance(coverR, coverC, enemy.pos.r, enemy.pos.c) <= distToEnemy) {
					hasCover = true;
				}
			}
		}
	}

	double teammatePenalty = 0;
	for (const auto* other : agents) {
		if (other->hp > 0 && other->team == me.team && other->id != me.id) {
			double dist = ManhattanDistance(r, c, other->pos.r, other->pos.c);
			if (dist <= 1.0) teammatePenalty -= 80.0;
			else if (dist <= 2.0) teammatePenalty -= 20.0;
		}
	}
	score += teammatePenalty;

	if (me.role == FIGHTER) {
		if (isLastStand) {
			if (hasLOS) score += 150.0;
			score -= distToEnemy * 10.0;
		}
		else {
			double hpRatio = (double)me.hp / 100.0;

			if (me.trait == AGGRESSIVE) {
				if (hpRatio > 0.6) {
					if (hasLOS) score += 120.0;
					else score -= 80.0;
					if (hasCover) score += 5.0;
				}
				else {
					if (hasLOS) score += 60.0;
					else score -= 20.0;
					if (hasCover) score += 30.0;
				}
			}
			else {
				if (hasLOS) score += 50.0;
				else score -= 10.0;
				if (hasCover) score += 80.0;
			}

			if (me.timeWithoutLOS > 6) {
				if (hasLOS) score += 200.0;
				if (hasCover && !hasLOS) score -= 60.0;
			}
		}
	}
	else {
		score += distToEnemy * 10.0;
		if (hasCover) score += 40.0;
		if (hasLOS) score -= 50.0;
	}

	if (isCurrentPos) score -= 5.0;
	score += (rand() % 100) / 100.0;
	return score;
}

static Agent* GetNearestEnemyGlobal(Agent& me) {
	Agent* nearest = nullptr;
	double minDist = 9999;
	for (auto* a : agents) {
		if (a->hp > 0 && a->team != me.team) {
			double d = ManhattanDistance(me.pos.r, me.pos.c, a->pos.r, a->pos.c);
			if (d < minDist) { minDist = d; nearest = a; }
		}
	}
	return nearest;
}

static Agent* GetVanguardFighter(Team team) {
	Agent* vanguard = nullptr;
	double minDistToEnemy = 9999;
	for (auto* a : agents) {
		if (a->hp > 0 && a->team == team && a->role == FIGHTER) {
			Agent* enemy = GetNearestEnemyGlobal(*a);
			if (enemy) {
				double d = ManhattanDistance(a->pos.r, a->pos.c, enemy->pos.r, enemy->pos.c);
				if (d < minDistToEnemy) {
					minDistToEnemy = d;
					vanguard = a;
				}
			}
		}
	}
	return vanguard;
}

static Agent* GetNearestFriendlyFighter(Agent& me) {
	Agent* nearest = nullptr;
	double minDist = 9999;
	for (auto* a : agents) {
		if (a->hp > 0 && a->team == me.team && a->role == FIGHTER && a->id != me.id) {
			double d = ManhattanDistance(me.pos.r, me.pos.c, a->pos.r, a->pos.c);
			if (d < minDist) { minDist = d; nearest = a; }
		}
	}
	return nearest;
}

static Agent* GetSupport(Team t, Role role) {
	for (auto* a : agents) {
		if (a->hp > 0 && a->team == t && a->role == role) return a;
	}
	return nullptr;
}

Point static GetNearestPickup(Point start, int pickupType) {
	Point best = { -1, -1 };
	double minDist = 9999;
	for (int i = 0; i < MSZ; i++) {
		for (int j = 0; j < MSZ; j++) {
			if (maze[i][j] == pickupType) {
				double d = ManhattanDistance(start.r, start.c, i, j);
				if (d < minDist) { minDist = d; best = { i, j }; }
			}
		}
	}
	return best;
}

void static DrawBitmapText(const char* str, double x, double y, void* font) {
	glRasterPos2d(x, y);
	for (const char* c = str; *c != '\0'; c++) glutBitmapCharacter(font, *c);
}

void static DrawCircle(double cx, double cy, double r, int segments) {
	glBegin(GL_TRIANGLE_FAN);
	glVertex2d(cx, cy);
	for (int i = 0; i <= segments; i++) {
		double angle = 2.0 * 3.1415926 * double(i) / double(segments);
		glVertex2d(cx + r * cos(angle), cy + r * sin(angle));
	}
	glEnd();
}

void static CreateRoom(int id, int r, int c, int h, int w) {
	rooms.push_back({ id, r, c, h, w });
	for (int i = r; i < r + h; i++) {
		for (int j = c; j < c + w; j++) {
			maze[i][j] = ROOM_FLOOR; roomMap[i][j] = id;
		}
	}
}

void static CreateCorridor(int r1, int c1, int r2, int c2) {
	int currR = r1, currC = c1;
	while (currR != r2) {
		if (maze[currR][currC] == WALL) maze[currR][currC] = SPACE;
		if (maze[currR][currC + 1] == WALL) maze[currR][currC + 1] = SPACE;
		currR += (r2 > currR) ? 1 : -1;
	}
	while (currC != c2) {
		if (maze[currR][currC] == WALL) maze[currR][currC] = SPACE;
		if (maze[currR + 1][currC] == WALL) maze[currR + 1][currC] = SPACE;
		currC += (c2 > currC) ? 1 : -1;
	}
}

void static AddAgent(Team t, Role r, int roomIndex) {
	Room room = rooms[roomIndex];
	int startR, startC;
	do {
		startR = room.r + 1 + rand() % (room.h - 2);
		startC = room.c + 1 + rand() % (room.w - 2);
	} while (maze[startR][startC] != ROOM_FLOOR || IsOccupiedByAgent(startR, startC, -1));

	Agent* newAgent = new Agent(agentIdCounter++, t, r, startR, startC);
	agents.push_back(newAgent);
}

void static InitMaze() {
	gameOver = false;
	extraDelay = 0;
	turnsWithoutDamage = 0;
	isLastStand = false;
	combatLog.clear();

	for (auto* a : agents) delete a;
	agents.clear();
	agentIdCounter = 0;

	for (int i = 0; i < MSZ; i++) {
		for (int j = 0; j < MSZ; j++) { maze[i][j] = WALL; roomMap[i][j] = 0; }
	}
	rooms.clear();
	CreateRoom(1, 10, 10, 20, 20);
	CreateRoom(2, 10, 70, 20, 20);
	CreateRoom(3, 70, 10, 20, 20);
	CreateRoom(4, 70, 70, 20, 20);
	CreateRoom(5, 40, 40, 20, 20);

	CreateCorridor(20, 20, 50, 50); CreateCorridor(20, 80, 50, 50);
	CreateCorridor(80, 20, 50, 50); CreateCorridor(80, 80, 50, 50);

	for (auto& room : rooms) {
		int numCovers = 8 + rand() % 4;
		for (int k = 0; k < numCovers; k++) {
			int cr = room.r + 2 + rand() % (room.h - 4), cc = room.c + 2 + rand() % (room.w - 4);
			maze[cr][cc] = COVER; maze[cr + 1][cc] = COVER;
			if (rand() % 2 == 0) maze[cr][cc + 1] = COVER;
		}
	}

	for (int k = 0; k < 2; k++) {
		int hr, hc;
		do {
			Room& r = rooms[rand() % rooms.size()];
			hr = r.r + rand() % r.h; hc = r.c + rand() % r.w;
		} while (maze[hr][hc] != ROOM_FLOOR);
		maze[hr][hc] = HEALTH_PICKUP;
	}

	for (int k = 0; k < 2; k++) {
		int ar, ac;
		do {
			Room& r = rooms[rand() % rooms.size()];
			ar = r.r + rand() % r.h; ac = r.c + rand() % r.w;
		} while (maze[ar][ac] != ROOM_FLOOR);
		maze[ar][ac] = AMMO_PICKUP;
	}

	int roomTeamA = rand() % rooms.size();
	int roomTeamB;
	do { roomTeamB = rand() % rooms.size(); } while (roomTeamA == roomTeamB);

	AddAgent(TEAM_A, FIGHTER, roomTeamA); AddAgent(TEAM_A, FIGHTER, roomTeamA);
	AddAgent(TEAM_A, MEDIC, roomTeamA); AddAgent(TEAM_A, SUPPLIER, roomTeamA);

	AddAgent(TEAM_B, FIGHTER, roomTeamB); AddAgent(TEAM_B, FIGHTER, roomTeamB);
	AddAgent(TEAM_B, MEDIC, roomTeamB); AddAgent(TEAM_B, SUPPLIER, roomTeamB);

	AddLog("Game Started!", TEAM_A);
}

Point static RunAStar(Point start, Point target, int myId) {
	if (start == target) return start;
	priority_queue<Cell*, vector<Cell*>, CompareCells> pq;
	bool closedList[MSZ][MSZ] = { false };
	Cell* startNode = new Cell(start.r, start.c, nullptr, 0, ManhattanDistance(start.r, start.c, target.r, target.c));
	pq.push(startNode);
	Cell* current = nullptr; Cell* targetNode = nullptr; bool found = false;

	while (!pq.empty()) {
		current = pq.top(); pq.pop();
		if (closedList[current->getRow()][current->getCol()]) continue;
		closedList[current->getRow()][current->getCol()] = true;

		if (current->getRow() == target.r && current->getCol() == target.c) { targetNode = current; found = true; break; }

		int dr[4] = { -1, 1, 0, 0 }; int dc[4] = { 0, 0, -1, 1 };
		for (int i = 0; i < 4; i++) {
			int nr = current->getRow() + dr[i]; int nc = current->getCol() + dc[i];
			if (IsValid(nr, nc) && !closedList[nr][nc]) {
				pq.push(new Cell(nr, nc, current, current->getG() + 1, ManhattanDistance(nr, nc, target.r, target.c)));
			}
		}
	}
	Point nextStep = start;
	if (found && targetNode != nullptr) {
		Cell* path = targetNode;
		while (path != nullptr && path->getParent() != nullptr && path->getParent()->getParent() != nullptr) path = path->getParent();
		if (path != nullptr) { nextStep.r = path->getRow(); nextStep.c = path->getCol(); }
	}
	return nextStep;
}

void static UpdateGame() {
	if (gameOver) return;

	currentEffects.clear();
	bool grenadeThrownThisTurn = false;
	bool shotFiredThisTurn = false;

	int fightersA = 0, fightersB = 0;
	for (auto* a : agents) {
		if (a->hp > 0 && a->role == FIGHTER) {
			if (a->team == TEAM_A) fightersA++;
			else fightersB++;
		}
	}

	if (fightersA == 0) { gameOver = true; winningTeam = TEAM_B; return; }
	if (fightersB == 0) { gameOver = true; winningTeam = TEAM_A; return; }

	turnsWithoutDamage++;
	if (turnsWithoutDamage > 40 && !isLastStand) {
		isLastStand = true;
		AddLog("LAST STAND ACTIVATED! NO COVER!", TEAM_A);
	}

	auto MoveAgent = [&](Agent* a, Point nextStep) {
		bool isBlocked = false;
		Agent* blockingAgent = nullptr;

		if (nextStep == a->pos) {
			isBlocked = true;
		}
		else {
			for (auto* other : agents) {
				if (other->hp > 0 && other->id != a->id && other->pos == nextStep) {
					blockingAgent = other;
					isBlocked = true;
					break;
				}
			}
		}

		if (!isBlocked) {
			a->pos = nextStep;
			a->stuckCounter = 0;
		}
		else {
			a->stuckCounter++;

			if (a->stuckCounter >= 3 && roomMap[a->pos.r][a->pos.c] == 0 && !a->isForcedRetreat) {
				a->isForcedRetreat = true;
				double minDist = 9999;
				for (auto& r : rooms) {
					double dToRoom = ManhattanDistance(a->pos.r, a->pos.c, r.r + r.h / 2, r.c + r.w / 2);
					if (dToRoom < minDist) {
						minDist = dToRoom;
						a->currentTarget = { r.r + r.h / 2, r.c + r.w / 2 };
					}
				}
				a->stuckCounter = 0;
				string tName = (a->team == TEAM_A) ? "Red" : "Blue";
				AddLog(tName + " unit falls back to clear path!", a->team);
				return;
			}

			if (blockingAgent && (a->stuckCounter == 2 || a->stuckCounter == 4)) {
				Point temp = a->pos;
				a->pos = blockingAgent->pos;
				blockingAgent->pos = temp;
			}
			else if (a->stuckCounter >= 5) {
				int dr[4] = { -1, 1, 0, 0 }; int dc[4] = { 0, 0, -1, 1 };
				vector<Point> safeMoves;
				for (int i = 0; i < 4; i++) {
					int nr = a->pos.r + dr[i]; int nc = a->pos.c + dc[i];
					if (IsValid(nr, nc) && !IsOccupiedByAgent(nr, nc, a->id)) {
						safeMoves.push_back({ nr, nc });
					}
				}
				if (!safeMoves.empty()) {
					a->pos = safeMoves[rand() % safeMoves.size()];
				}
				a->stuckCounter = 0;
			}
		}
		};

	for (auto* a : agents) {
		if (a->hp <= 0) continue;

		int myRoom = roomMap[a->pos.r][a->pos.c];

		if (a->isForcedRetreat) {
			if (myRoom != 0) {
				a->isForcedRetreat = false;
			}
			else {
				MoveAgent(a, RunAStar(a->pos, a->currentTarget, a->id));
				continue;
			}
		}

		Agent* targetEnemyInRoom = nullptr;
		double minEnemyDist = 9999;

		if (myRoom != 0) {
			for (auto* e : agents) {
				if (e->hp > 0 && e->team != a->team && roomMap[e->pos.r][e->pos.c] == myRoom) {
					double d = ManhattanDistance(a->pos.r, a->pos.c, e->pos.r, e->pos.c);
					if (d < minEnemyDist) { minEnemyDist = d; targetEnemyInRoom = e; }
				}
			}
		}

		bool canHeal = (GetSupport(a->team, MEDIC) != nullptr) || (GetNearestPickup(a->pos, HEALTH_PICKUP).r != -1);
		bool canAmmo = (GetSupport(a->team, SUPPLIER) != nullptr) || (GetNearestPickup(a->pos, AMMO_PICKUP).r != -1);

		bool needsHeal = (a->hp <= a->fleeThreshold * 100) && canHeal;
		bool needsAmmo = (a->role == FIGHTER && a->ammo <= a->ammoThreshold) && canAmmo;

		if (a->role == FIGHTER) {
			if (needsHeal) a->state = STATE_SEEK_MEDIC;
			else if (needsAmmo) a->state = STATE_SEEK_AMMO;
			else if (targetEnemyInRoom) a->state = STATE_COMBAT;
			else a->state = STATE_HUNT;
		}
		else {
			if (targetEnemyInRoom) a->state = STATE_FLEE;
			else a->state = STATE_HUNT;
		}

		if (a->state == STATE_SEEK_MEDIC) {
			Point targetPos = { -1, -1 };
			double minDist = 9999;
			bool isPickup = false;

			Agent* myMedic = GetSupport(a->team, MEDIC);
			if (myMedic) {
				minDist = ManhattanDistance(a->pos.r, a->pos.c, myMedic->pos.r, myMedic->pos.c);
				targetPos = myMedic->pos;
			}

			Point pickupPos = GetNearestPickup(a->pos, HEALTH_PICKUP);
			if (pickupPos.r != -1) {
				double pDist = ManhattanDistance(a->pos.r, a->pos.c, pickupPos.r, pickupPos.c);
				if (pDist < minDist) { targetPos = pickupPos; isPickup = true; }
			}

			if (targetPos.r != -1) {
				if (ManhattanDistance(a->pos.r, a->pos.c, targetPos.r, targetPos.c) <= 1) {
					a->hp = 100;
					if (isPickup) maze[targetPos.r][targetPos.c] = ROOM_FLOOR;
					AddLog((a->team == TEAM_A ? "Red" : "Blue") + string(" team unit healed!"), a->team);
				}
				else {
					MoveAgent(a, RunAStar(a->pos, targetPos, a->id));
				}
			}
		}

		else if (a->state == STATE_SEEK_AMMO) {
			Point targetPos = { -1, -1 };
			double minDist = 9999;
			bool isPickup = false;

			Agent* mySupplier = GetSupport(a->team, SUPPLIER);
			if (mySupplier) {
				minDist = ManhattanDistance(a->pos.r, a->pos.c, mySupplier->pos.r, mySupplier->pos.c);
				targetPos = mySupplier->pos;
			}

			Point pickupPos = GetNearestPickup(a->pos, AMMO_PICKUP);
			if (pickupPos.r != -1) {
				double pDist = ManhattanDistance(a->pos.r, a->pos.c, pickupPos.r, pickupPos.c);
				if (pDist < minDist) { targetPos = pickupPos; isPickup = true; }
			}

			if (targetPos.r != -1) {
				if (ManhattanDistance(a->pos.r, a->pos.c, targetPos.r, targetPos.c) <= 1) {
					a->ammo = 30;
					if (isPickup) maze[targetPos.r][targetPos.c] = ROOM_FLOOR;
					AddLog((a->team == TEAM_A ? "Red" : "Blue") + string(" Fighter reloaded!"), a->team);
				}
				else {
					MoveAgent(a, RunAStar(a->pos, targetPos, a->id));
				}
			}
		}

		else if (a->state == STATE_COMBAT) {
			bool los = HasLineOfSight(a->pos.r, a->pos.c, targetEnemyInRoom->pos.r, targetEnemyInRoom->pos.c);

			if (!los) a->timeWithoutLOS++;
			else a->timeWithoutLOS = 0;

			if (a->ammo > 0) {
				bool throwGrenade = false;

				if (a->ammo >= 2 && minEnemyDist <= 8) {
					if (!los) throwGrenade = true;
					else if (a->trait == AGGRESSIVE && (rand() % 100 < 30)) throwGrenade = true;
				}

				if (throwGrenade) {
					targetEnemyInRoom->hp -= 35;
					a->ammo -= 2;
					turnsWithoutDamage = 0;
					currentEffects.push_back({ a->pos, targetEnemyInRoom->pos, true });
					grenadeThrownThisTurn = true;
					AddLog((a->team == TEAM_A ? "Red" : "Blue") + string(" threw a Grenade!"), a->team);

					if (targetEnemyInRoom->hp <= 0) AddLog("Enemy killed by Grenade!", a->team);

				}
				else if (los) {
					targetEnemyInRoom->hp -= 20;
					a->ammo--;
					turnsWithoutDamage = 0;
					currentEffects.push_back({ a->pos, targetEnemyInRoom->pos, false });
					shotFiredThisTurn = true;
					AddLog((a->team == TEAM_A ? "Red" : "Blue") + string(" shoots!"), a->team);

					if (targetEnemyInRoom->hp <= 0) AddLog("Enemy killed by Shot!", a->team);
				}
			}
			else {
				if (minEnemyDist <= 2.0) {
					targetEnemyInRoom->hp -= 25;
					turnsWithoutDamage = 0;
					currentEffects.push_back({ a->pos, targetEnemyInRoom->pos, false });
					shotFiredThisTurn = true;
					AddLog((a->team == TEAM_A ? "Red" : "Blue") + string(" MELEE ATTACK!"), a->team);
				}
			}

			Point bestMove = a->pos;
			double bestSafety = -9999;
			int dr[5] = { 0, -1, 1, 0, 0 };
			int dc[5] = { 0, 0, 0, -1, 1 };
			for (int i = 0; i < 5; i++) {
				int nr = a->pos.r + dr[i]; int nc = a->pos.c + dc[i];
				if (IsValid(nr, nc) && roomMap[nr][nc] == myRoom && (!IsOccupiedByAgent(nr, nc, a->id) || i == 0)) {
					double safety = GetSafetyScore(nr, nc, *a, *targetEnemyInRoom, (i == 0));
					if (safety > bestSafety) { bestSafety = safety; bestMove = { nr, nc }; }
				}
			}
			a->pos = bestMove;
			a->stuckCounter = 0;
		}

		else if (a->state == STATE_HUNT) {
			Agent* target = nullptr;
			if (a->role == FIGHTER) target = GetNearestEnemyGlobal(*a);
			else target = GetVanguardFighter(a->team);

			if (target) {
				int tRoomId = roomMap[target->pos.r][target->pos.c];

				if (a->role == FIGHTER) {
					if (myRoom == 0 && tRoomId == 0 && ManhattanDistance(a->pos.r, a->pos.c, target->pos.r, target->pos.c) <= 6) {
						if (a->team == TEAM_A) {
							double minDistToRoom = 9999;
							for (auto& r : rooms) {
								double dToRoom = ManhattanDistance(a->pos.r, a->pos.c, r.r + r.h / 2, r.c + r.w / 2);
								if (dToRoom < minDistToRoom) {
									minDistToRoom = dToRoom;
									a->currentTarget = { r.r + r.h / 2, r.c + r.w / 2 };
								}
							}
						}
						else {
							a->currentTarget = target->pos;
						}
					}
					else if (myRoom != 0 && tRoomId == 0) {
						if (ManhattanDistance(a->pos.r, a->pos.c, target->pos.r, target->pos.c) <= 4) {
							for (auto& r : rooms) {
								if (r.id == myRoom) { a->currentTarget = { r.r + r.h / 2, r.c + r.w / 2 }; break; }
							}
						}
						else {
							a->currentTarget = target->pos;
						}
					}
					else {
						a->currentTarget = target->pos;
					}
				}
				else {
					if (tRoomId != 0 && myRoom != tRoomId) {
						for (auto& r : rooms) {
							if (r.id == tRoomId) { a->currentTarget = { r.r + r.h / 2, r.c + r.w / 2 }; break; }
						}
					}
					else {
						a->currentTarget = target->pos;
					}
				}

				if (a->currentTarget != a->pos && a->currentTarget.r != -1) {
					MoveAgent(a, RunAStar(a->pos, a->currentTarget, a->id));
				}
			}
		}

		else if (a->state == STATE_FLEE) {
			if (a->currentTarget.r == -1 || roomMap[a->currentTarget.r][a->currentTarget.c] == myRoom) {
				int badRoom = targetEnemyInRoom ? roomMap[targetEnemyInRoom->pos.r][targetEnemyInRoom->pos.c] : myRoom;
				Room targetRoom;
				do {
					targetRoom = rooms[rand() % rooms.size()];
				} while (targetRoom.id == myRoom || targetRoom.id == badRoom);

				a->currentTarget = { targetRoom.r + targetRoom.h / 2, targetRoom.c + targetRoom.w / 2 };
			}
			MoveAgent(a, RunAStar(a->pos, a->currentTarget, a->id));
		}
	}

	if (grenadeThrownThisTurn) {
		PlaySound(TEXT("grenade.wav"), NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
		extraDelay = 1200;
	}
	else if (shotFiredThisTurn) {
		PlaySound(TEXT("shoot.wav"), NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
		extraDelay = 0;
	}
	else {
		extraDelay = 0;
	}
}


void static DrawInfluenceMap() {
	if (!showInfluenceMap) return;
	for (int i = 0; i < MSZ; i++) {
		for (int j = 0; j < MSZ; j++) {
			if (maze[i][j] == ROOM_FLOOR) {
				double danger = CalculateCellDanger(i, j, TEAM_A);
				if (danger > 0) {
					// כמה שיותר מסוכן, כך ייצבע יותר באדום. ירוק משמעו בטוח.
					glColor3d(min(1.0, danger * 5.0), 1.0 - min(1.0, danger * 5.0), 0);
					glRectd(j, i, j + 1, i + 1);
				}
			}
		}
	}
}

void static DrawLegend() {
	double lx = MSZ + 2, ly = MSZ - 3;
	glColor3d(1, 1, 1); DrawBitmapText("--- LEGEND ---", lx, ly, GLUT_BITMAP_HELVETICA_18); ly -= 3.5;
	glColor3d(0.1, 0.1, 0.1); glRectd(lx, ly, lx + 2, ly + 2); glColor3d(1, 1, 1); DrawBitmapText("Wall", lx + 3, ly + 0.5, GLUT_BITMAP_HELVETICA_12); ly -= 3.5;
	glColor3d(0.3, 0.3, 0.3); glRectd(lx, ly, lx + 2, ly + 2); glColor3d(1, 1, 1); DrawBitmapText("Corridor", lx + 3, ly + 0.5, GLUT_BITMAP_HELVETICA_12); ly -= 3.5;
	glColor3d(0.7, 0.7, 0.7); glRectd(lx, ly, lx + 2, ly + 2); glColor3d(1, 1, 1); DrawBitmapText("Room Floor", lx + 3, ly + 0.5, GLUT_BITMAP_HELVETICA_12); ly -= 3.5;
	glColor3d(0.5, 0.25, 0.0); glRectd(lx, ly, lx + 2, ly + 2); glColor3d(1, 1, 1); DrawBitmapText("Cover", lx + 3, ly + 0.5, GLUT_BITMAP_HELVETICA_12); ly -= 3.5;
	glColor3d(0.0, 1.0, 0.0); glRectd(lx, ly, lx + 2, ly + 2); glColor3d(1, 1, 1); DrawBitmapText("Health Kit", lx + 3, ly + 0.5, GLUT_BITMAP_HELVETICA_12); ly -= 3.5;
	glColor3d(1.0, 0.8, 0.0); glRectd(lx, ly, lx + 2, ly + 2); glColor3d(1, 1, 1); DrawBitmapText("Ammo Box", lx + 3, ly + 0.5, GLUT_BITMAP_HELVETICA_12); ly -= 5.5;

	glColor3d(1, 1, 1); DrawBitmapText("--- COMBAT ---", lx, ly, GLUT_BITMAP_HELVETICA_18); ly -= 3.5;
	glColor3d(1, 1, 0); DrawCircle(lx + 1, ly + 1, 1.0, 10); glColor3d(1, 1, 1); DrawBitmapText("Shoot (Line)", lx + 3, ly + 0.5, GLUT_BITMAP_HELVETICA_12); ly -= 3.5;
	glColor3d(1, 0.5, 0); DrawCircle(lx + 1, ly + 1, 1.0, 10); glColor3d(1, 1, 1); DrawBitmapText("Grenade (Circle)", lx + 3, ly + 0.5, GLUT_BITMAP_HELVETICA_12); ly -= 5.5;

	glColor3d(1, 1, 1); DrawBitmapText("--- ENTITIES ---", lx, ly, GLUT_BITMAP_HELVETICA_18); ly -= 3.5;
	glColor3d(1, 0, 0); DrawCircle(lx + 1, ly + 1, 1.0, 10); glColor3d(1, 1, 1); DrawBitmapText("Team A (Red)", lx + 3, ly + 0.5, GLUT_BITMAP_HELVETICA_12); ly -= 3.5;
	glColor3d(0, 0, 1); DrawCircle(lx + 1, ly + 1, 1.0, 10); glColor3d(1, 1, 1); DrawBitmapText("Team B (Blue)", lx + 3, ly + 0.5, GLUT_BITMAP_HELVETICA_12); ly -= 6;

	glColor3d(1, 1, 1); DrawBitmapText("Roles & Personality:", lx, ly, GLUT_BITMAP_HELVETICA_12); ly -= 3.5;
	glColor3d(1, 1, 1); DrawBitmapText("F(A) = Aggressive Fighter", lx, ly, GLUT_BITMAP_HELVETICA_12); ly -= 3.5;
	glColor3d(1, 1, 1); DrawBitmapText("F(C) = Cautious Fighter", lx, ly, GLUT_BITMAP_HELVETICA_12); ly -= 3.5;
	glColor3d(1, 1, 1); DrawBitmapText("M = Medic, S = Supplier", lx, ly, GLUT_BITMAP_HELVETICA_12); ly -= 5.5;

	glColor3d(1, 1, 1); DrawBitmapText("--- STATUS BARS ---", lx, ly, GLUT_BITMAP_HELVETICA_18); ly -= 3.5;

	glColor3d(1, 0, 0); glRectd(lx, ly, lx + 2, ly + 0.8);
	glColor3d(0, 1, 0); glRectd(lx, ly, lx + 1.2, ly + 0.8);
	glColor3d(1, 1, 1); DrawBitmapText("Health (HP)", lx + 3, ly + 0.2, GLUT_BITMAP_HELVETICA_12); ly -= 3.5;

	glColor3d(0, 0, 0); glRectd(lx, ly, lx + 2, ly + 0.8);
	glColor3d(0, 0, 1); glRectd(lx, ly, lx + 1.5, ly + 0.8);
	glColor3d(1, 1, 1); DrawBitmapText("Ammo (Fighters)", lx + 3, ly + 0.2, GLUT_BITMAP_HELVETICA_12); ly -= 5.5;

	glColor3d(1, 1, 0); DrawBitmapText("Press 'M' to toggle", lx, ly, GLUT_BITMAP_HELVETICA_18); ly -= 2.5;
	DrawBitmapText("Influence Map view.", lx, ly, GLUT_BITMAP_HELVETICA_18);
}

void static DrawCombatLog() {
	double lx = 40.0;
	double ly = 95.0;

	if (!combatLog.empty()) {
		glColor3d(1, 1, 1);
		DrawBitmapText("--- LIVE COMBAT LOG ---", lx, ly, GLUT_BITMAP_HELVETICA_18);
		ly -= 3;

		for (auto& log : combatLog) {
			if (log.text == "Game Started!") glColor3d(1, 1, 1);
			else if (log.text == "LAST STAND ACTIVATED! NO COVER!") glColor3d(1, 0.8, 0);
			else if (log.team == TEAM_A) glColor3d(1, 0.4, 0.4);
			else glColor3d(0.4, 0.6, 1.0);

			DrawBitmapText(log.text.c_str(), lx, ly, GLUT_BITMAP_HELVETICA_18);
			ly -= 2.5;
		}
	}
}

void static display() {
	glClear(GL_COLOR_BUFFER_BIT);
	double x, y;

	// ציור המבוך
	for (int i = 0; i < MSZ; i++) {
		for (int j = 0; j < MSZ; j++) {
			x = j; y = i;
			switch (maze[i][j]) {
			case WALL: glColor3d(0.1, 0.1, 0.1); break;
			case SPACE: glColor3d(0.3, 0.3, 0.3); break;
			case ROOM_FLOOR: glColor3d(0.7, 0.7, 0.7); break;
			case COVER: glColor3d(0.5, 0.25, 0.0); break;
			case HEALTH_PICKUP: glColor3d(0.0, 1.0, 0.0); break;
			case AMMO_PICKUP: glColor3d(1.0, 0.8, 0.0); break;
			}
			glBegin(GL_POLYGON);
			glVertex2d(x, y); glVertex2d(x, y + 1); glVertex2d(x + 1, y + 1); glVertex2d(x + 1, y);
			glEnd();
			if (maze[i][j] == COVER) {
				glColor3d(0, 0, 0); glBegin(GL_LINE_LOOP);
				glVertex2d(x, y); glVertex2d(x, y + 1); glVertex2d(x + 1, y + 1); glVertex2d(x + 1, y); glEnd();
			}
		}
	}

	DrawInfluenceMap();

	for (const auto* a : agents) {
		if (a->hp <= 0) continue;
		double cx = a->pos.c + 0.5, cy = a->pos.r + 0.5;

		if (a->team == TEAM_A) glColor3d(1, 0, 0);
		else glColor3d(0, 0, 1);

		DrawCircle(cx, cy, 1.0, 15);

		glColor3d(1, 1, 1);
		if (a->role == FIGHTER) {
			if (a->trait == AGGRESSIVE) DrawBitmapText("F(A)", cx - 0.7, cy - 0.3, GLUT_BITMAP_HELVETICA_12);
			else DrawBitmapText("F(C)", cx - 0.7, cy - 0.3, GLUT_BITMAP_HELVETICA_12);
		}
		else if (a->role == MEDIC) DrawBitmapText("M", cx - 0.4, cy - 0.3, GLUT_BITMAP_HELVETICA_12);
		else if (a->role == SUPPLIER) DrawBitmapText("S", cx - 0.3, cy - 0.3, GLUT_BITMAP_HELVETICA_12);

		glColor3d(1, 0, 0); glRectd(cx - 1.0, cy + 1.2, cx + 1.0, cy + 1.5);
		glColor3d(0, 1, 0); double hpRatio = (double)a->hp / 100.0;
		if (hpRatio > 0) glRectd(cx - 1.0, cy + 1.2, cx - 1.0 + (2.0 * hpRatio), cy + 1.5);

		glColor3d(1, 1, 1);
		string hpText = to_string(a->hp);
		DrawBitmapText(hpText.c_str(), cx - 0.5, cy + 1.8, GLUT_BITMAP_HELVETICA_12);

		if (a->role == FIGHTER) {
			glColor3d(0, 0, 0); glRectd(cx - 1.0, cy - 1.5, cx + 1.0, cy - 1.2);
			glColor3d(0, 0, 1); double ammoRatio = (double)a->ammo / 30.0;
			if (ammoRatio > 0) glRectd(cx - 1.0, cy - 1.5, cx - 1.0 + (2.0 * ammoRatio), cy - 1.2);
		}
	}

	// ציור אפקטים (יריות ורימונים)
	for (auto& eff : currentEffects) {
		double startX = eff.start.c + 0.5, startY = eff.start.r + 0.5;
		double targetX = eff.end.c + 0.5, targetY = eff.end.r + 0.5;
		if (eff.isGrenade) {
			glColor3d(1, 0.5, 0); DrawCircle(targetX, targetY, 1.2, 20);
		}
		else {
			glColor3d(1, 1, 0); glLineWidth(3.0); glBegin(GL_LINES);
			glVertex2d(startX, startY); glVertex2d(targetX, targetY); glEnd(); glLineWidth(1.0);
		}
	}

	DrawLegend();
	if (!gameOver) DrawCombatLog();

	if (gameOver) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glColor4d(0, 0, 0, 0.85);
		glRectd(0, 0, MSZ + UI_WIDTH, MSZ);
		glDisable(GL_BLEND);

		glColor3d(0.2, 0.8, 0.2);
		glRectd(BTN_X1, BTN_Y1, BTN_X2, BTN_Y2);
		glColor3d(1, 1, 1);
		glBegin(GL_LINE_LOOP);
		glVertex2d(BTN_X1, BTN_Y1); glVertex2d(BTN_X2, BTN_Y1);
		glVertex2d(BTN_X2, BTN_Y2); glVertex2d(BTN_X1, BTN_Y2);
		glEnd();
		DrawBitmapText("RESTART GAME", BTN_X1 + 5.0, BTN_Y1 + 2.8, GLUT_BITMAP_HELVETICA_18);

		double centerX = (MSZ + UI_WIDTH) / 2.0;
		double startY = MSZ / 2.0 + 15;

		if (winningTeam == TEAM_A) {
			glColor3d(1, 0.2, 0.2);
			DrawBitmapText("TEAM A (RED) WINS THE BATTLE!", centerX - 21.5, startY, GLUT_BITMAP_TIMES_ROMAN_24);
		}
		else {
			glColor3d(0.2, 0.6, 1);
			DrawBitmapText("TEAM B (BLUE) WINS THE BATTLE!", centerX - 21.5, startY, GLUT_BITMAP_TIMES_ROMAN_24);
		}

		startY -= 8;
		glColor3d(1, 1, 1);
		DrawBitmapText("SURVIVORS STATISTICS:", centerX - 11, startY, GLUT_BITMAP_HELVETICA_18);

		startY -= 4;
		glColor3d(0.8, 0.8, 0.8);

		double col1 = centerX - 22;
		double col2 = centerX - 8;
		double col3 = centerX + 6;
		double col4 = centerX + 14;

		DrawBitmapText("ROLE", col1, startY, GLUT_BITMAP_HELVETICA_18);
		DrawBitmapText("|  TRAIT", col2, startY, GLUT_BITMAP_HELVETICA_18);
		DrawBitmapText("|  HP", col3, startY, GLUT_BITMAP_HELVETICA_18);
		DrawBitmapText("|  AMMO", col4, startY, GLUT_BITMAP_HELVETICA_18);

		startY -= 1.5;
		glBegin(GL_LINES); glVertex2d(centerX - 24, startY); glVertex2d(centerX + 24, startY); glEnd();

		for (auto* a : agents) {
			if (a->team == winningTeam && a->hp > 0) {
				startY -= 3.5;
				string roleStr = (a->role == FIGHTER) ? "Fighter" : (a->role == MEDIC) ? "Medic" : "Supplier";
				string traitStr = (a->trait == AGGRESSIVE) ? "Aggressive" : (a->trait == CAUTIOUS) ? "Cautious" : "Support";

				string hpStr = to_string(a->hp);
				string ammoStr = (a->role == FIGHTER) ? to_string(a->ammo) : "--";

				if (winningTeam == TEAM_A) glColor3d(1, 0.5, 0.5); else glColor3d(0.5, 0.8, 1);

				DrawBitmapText(roleStr.c_str(), col1, startY, GLUT_BITMAP_HELVETICA_18);
				DrawBitmapText((string("|  ") + traitStr).c_str(), col2, startY, GLUT_BITMAP_HELVETICA_18);
				DrawBitmapText((string("|  ") + hpStr).c_str(), col3, startY, GLUT_BITMAP_HELVETICA_18);
				DrawBitmapText((string("|  ") + ammoStr).c_str(), col4, startY, GLUT_BITMAP_HELVETICA_18);
			}
		}

		glColor3d(1, 1, 1);
		DrawBitmapText("Click 'RESTART GAME' on the right panel to play again.", centerX - 25, startY - 10, GLUT_BITMAP_HELVETICA_18);
	}
	else {
		glColor3d(0.2, 0.6, 0.2);
		glRectd(BTN_X1, BTN_Y1, BTN_X2, BTN_Y2);
		glColor3d(1, 1, 1);
		glBegin(GL_LINE_LOOP);
		glVertex2d(BTN_X1, BTN_Y1); glVertex2d(BTN_X2, BTN_Y1);
		glVertex2d(BTN_X2, BTN_Y2); glVertex2d(BTN_X1, BTN_Y2);
		glEnd();
		DrawBitmapText("RESTART GAME", BTN_X1 + 5.0, BTN_Y1 + 2.8, GLUT_BITMAP_HELVETICA_18);
	}

	glutSwapBuffers();
}

void static idle() {
	Sleep(300 + extraDelay);
	extraDelay = 0;
	UpdateGame();
	glutPostRedisplay();
}

void static reshape(int w, int h) {
	windowWidth = w;
	windowHeight = h;
	glViewport(0, 0, w, h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, MSZ + UI_WIDTH, 0, MSZ, -1, 1);
	glMatrixMode(GL_MODELVIEW);
}

void static mouse(int button, int state, int x, int y) {
	if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
		double logicalX = (double)x / windowWidth * (MSZ + UI_WIDTH);
		double logicalY = (double)(windowHeight - y) / windowHeight * MSZ;

		if (logicalX >= BTN_X1 && logicalX <= BTN_X2 &&
			logicalY >= BTN_Y1 && logicalY <= BTN_Y2) {
			InitMaze();
			glutPostRedisplay();
		}
	}
}

// --- האזנה למקלדת ---
void static keyboard(unsigned char key, int x, int y) {
	if (key == 'm' || key == 'M' || key == 246) {
		showInfluenceMap = !showInfluenceMap;
		glutPostRedisplay();
	}
}

int main(int argc, char* argv[]) {
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE);
	glutInitWindowSize(windowWidth, windowHeight);

	// --- חישוב אמצע המסך הדינמי ---
	int screenWidth = glutGet(GLUT_SCREEN_WIDTH);
	int screenHeight = glutGet(GLUT_SCREEN_HEIGHT);

	int windowPosX = (screenWidth - windowWidth) / 2;
	int windowPosY = (screenHeight - windowHeight) / 2;

	glutInitWindowPosition(windowPosX, windowPosY);

	glutCreateWindow("Final AI Project - Modular");

	glutDisplayFunc(display);
	glutReshapeFunc(reshape);
	glutMouseFunc(mouse);
	glutKeyboardFunc(keyboard);
	glutIdleFunc(idle);

	srand((unsigned int)time(0));
	InitMaze();

	glutMainLoop();
	return 0;
}