#pragma once
#include <vector>
#include <string>

enum Team { TEAM_A, TEAM_B };
enum Role { FIGHTER, MEDIC, SUPPLIER };
enum State { STATE_HUNT, STATE_COMBAT, STATE_FLEE, STATE_SEEK_MEDIC, STATE_SEEK_AMMO };
enum Personality { AGGRESSIVE, CAUTIOUS, SUPPORT };

struct Point {
	int r, c;
	bool operator==(const Point& other) const { return r == other.r && c == other.c; }
	bool operator!=(const Point& other) const { return r != other.r || c != other.c; }
};

class Agent {
public:
	int id;
	Point pos;
	Team team;
	Role role;
	State state;
	Personality trait;
	int hp, ammo;
	double fleeThreshold;
	int ammoThreshold;
	Point currentTarget;
	int stuckCounter;
	int timeWithoutLOS;
	bool isForcedRetreat;

	Agent(int _id, Team _team, Role _role, int startR, int startC);
};