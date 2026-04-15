#include "Agent.h"
#include <cstdlib>

Agent::Agent(int _id, Team _team, Role _role, int startR, int startC)
	: id(_id), team(_team), role(_role), pos({ startR, startC }) {
	hp = 100;
	ammo = (role == FIGHTER) ? 30 : 0;
	state = STATE_HUNT;
	stuckCounter = 0;
	timeWithoutLOS = 0;
	isForcedRetreat = false;
	currentTarget = { -1, -1 };

	if (role == FIGHTER) {
		// הגדרת אופי רנדומלית לפי דרישות הפרויקט [cite: 8]
		trait = (rand() % 2 == 0) ? AGGRESSIVE : CAUTIOUS;
		fleeThreshold = (trait == AGGRESSIVE) ? 0.2 : 0.5;
		ammoThreshold = (trait == AGGRESSIVE) ? 0 : 10;
	}
	else {
		trait = SUPPORT;
		fleeThreshold = 0.8;
		ammoThreshold = 0;
	}
}