/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_PICKUP_H
#define GAME_SERVER_ENTITIES_PICKUP_H

#include <game/server/entity.h>

const int PickupPhysSize = 14;

class CPickup : public CEntity
{
public:
	CPickup(CGameWorld *pGameWorld, int Type, vec2 Pos, int MapID, bool Pickupable = true, int TeamSpecific=-1);

	virtual void Reset();
	virtual void Tick();
	virtual void TickPaused();
	virtual void Snap(int SnappingClient);
    void SetPos(vec2 pos) {m_Pos = pos;};

private:
    bool m_Pickupable;
	int m_Type;
	int m_SpawnTick;
    int m_Team;
};

#endif
