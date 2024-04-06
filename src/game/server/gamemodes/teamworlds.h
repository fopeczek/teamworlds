/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#pragma once
#include <game/server/gamecontroller.h>
#include <game/server/entity.h>

class CGameControllerCTFC : public IGameController
{
	// balancing
	virtual bool CanBeMovedOnBalance(int ClientID) const;

	// game
	class CFlag *m_apFlags[2];

	virtual bool DoWincheckMatch();

public:
	CGameControllerCTFC(class CGameContext *pGameServer);
	
	// event
	virtual int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon);
	virtual void OnFlagReturn(class CFlag *pFlag);
	virtual bool OnEntity(int Index, vec2 Pos, int MapID);

	// general
	virtual void Snap(int SnappingClient);
	virtual void Tick();
};
