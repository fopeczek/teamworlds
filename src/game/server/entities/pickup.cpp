/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <generated/server_data.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

#include "character.h"
#include "pickup.h"

CPickup::CPickup(CGameWorld *pGameWorld, int Type, vec2 Pos, int MapID, bool Pickupable, int TeamSpecific)
: CEntity(pGameWorld, CGameWorld::ENTTYPE_PICKUP, Pos, MapID, PickupPhysSize)
{
	m_Type = Type;

    m_Pickupable = Pickupable;

    m_Team=TeamSpecific;

	Reset();

	GameWorld()->InsertEntity(this);
}

void CPickup::Reset()
{
	if (g_pData->m_aPickups[m_Type].m_Spawndelay > 0)
		m_SpawnTick = Server()->Tick() + Server()->TickSpeed() * g_pData->m_aPickups[m_Type].m_Spawndelay;
	else
		m_SpawnTick = -1;
}

void CPickup::Tick()
{
    if(GetMapID()==Server()->LobbyMapID){
        m_SpawnTick = -1;
    }
    if (m_Pickupable) {
        // wait for respawn
        if (m_SpawnTick > 0) {
            if (Server()->Tick() > m_SpawnTick) {
                // respawn
                m_SpawnTick = -1;

                if (m_Type == PICKUP_GRENADE || m_Type == PICKUP_SHOTGUN || m_Type == PICKUP_LASER)
                    GameServer()->CreateSound(m_Pos, SOUND_WEAPON_SPAWN, -1, GetMapID());
            } else
                return;
        }
        // Check if a player intersected us
        CCharacter *pChr = (CCharacter *) GameWorld()->ClosestEntity(m_Pos, 20.0f, CGameWorld::ENTTYPE_CHARACTER, 0,
                                                                     GetMapID());
        if (pChr && pChr->IsAlive()) {
            // player picked us up, is someone was hooking us, let them go
            bool Picked = false;
            switch (m_Type) {
                case PICKUP_HEALTH:
                    if (GetMapID()!=Server()->LobbyMapID) {
                        if (pChr->IncreaseHealth(1)) {
                            Picked = true;
                            GameServer()->CreateSound(m_Pos, SOUND_PICKUP_HEALTH, -1, GetMapID());
                        }
                    }
                    break;

                case PICKUP_ARMOR:
                    if (GetMapID()!=Server()->LobbyMapID) {
                        if (pChr->IncreaseArmor(1)) {
                            Picked = true;
                            GameServer()->CreateSound(m_Pos, SOUND_PICKUP_ARMOR, -1, GetMapID());
                        }
                    }
                    break;

                case PICKUP_GRENADE:
                    if (GetMapID()==Server()->LobbyMapID){
                        CPickup *Hp[2];
                        int num = GameWorld()->FindEntities(m_Pos, GetProximityRadius()*2.f, (CEntity**)Hp, 2, CGameWorld::ENTTYPE_PICKUP, Server()->LobbyMapID);
                        if (num > 1) {
                            for (int i = 0; i < 2; i++) {
                                if (Hp[i]->m_Type == PICKUP_ARMOR) {
                                    pChr->GetPlayer()->Become(Class::Tank);
                                }
                            }
                        }else {
                            pChr->GetPlayer()->Become(Class::Scout);
                        }
                    } else {
                        if (pChr->GiveWeapon(WEAPON_GRENADE, g_pData->m_Weapons.m_aId[WEAPON_GRENADE].m_Maxammo)) {
                            Picked = true;
                            GameServer()->CreateSound(m_Pos, SOUND_PICKUP_GRENADE, -1, GetMapID());
                            if (pChr->GetPlayer())
                                GameServer()->SendWeaponPickup(pChr->GetPlayer()->GetCID(), WEAPON_GRENADE);
                        }
                    }
                    break;
                case PICKUP_SHOTGUN:
                    if (GetMapID()==Server()->LobbyMapID){
                        CPickup *Hp[2];
                        int num = GameWorld()->FindEntities(m_Pos, GetProximityRadius()*2.f, (CEntity**)Hp, 2, CGameWorld::ENTTYPE_PICKUP, Server()->LobbyMapID);
                        if (num > 1){
                            for (int i=0; i<2; i++) {
                                if (Hp[i]->m_Type == PICKUP_HEALTH) {
                                    pChr->GetPlayer()->Become(Class::Medic);
                                } else if (Hp[i]->m_Type == PICKUP_ARMOR) {
                                    pChr->GetPlayer()->Become(Class::Armorer);
                                }
                            }
                        } else {
                            pChr->GetPlayer()->Become(Class::Spider);
                        }
                    } else {
                        if (pChr->GiveWeapon(WEAPON_SHOTGUN, g_pData->m_Weapons.m_aId[WEAPON_SHOTGUN].m_Maxammo)) {
                            Picked = true;
                            GameServer()->CreateSound(m_Pos, SOUND_PICKUP_SHOTGUN, -1, GetMapID());
                            if (pChr->GetPlayer())
                                GameServer()->SendWeaponPickup(pChr->GetPlayer()->GetCID(), WEAPON_SHOTGUN);
                        }
                    }
                    break;
                case PICKUP_LASER:
                    if (GetMapID()==Server()->LobbyMapID){
                        pChr->GetPlayer()->Become(Class::Engineer);
                    }else {
                        if (pChr->GiveWeapon(WEAPON_LASER, g_pData->m_Weapons.m_aId[WEAPON_LASER].m_Maxammo)) {
                            Picked = true;
                            GameServer()->CreateSound(m_Pos, SOUND_PICKUP_SHOTGUN, -1, GetMapID());
                            if (pChr->GetPlayer())
                                GameServer()->SendWeaponPickup(pChr->GetPlayer()->GetCID(), WEAPON_LASER);
                        }
                    }
                    break;

                case PICKUP_NINJA: {
                    if (GetMapID()==Server()->LobbyMapID){
                        pChr->GetPlayer()->Become(Class::Hunter);
                    } else {
                        Picked = true;
                        // activate ninja on target player
                        pChr->GiveNinja();

                        // loop through all players, setting their emotes
                        CCharacter *pC = static_cast<CCharacter *>(GameWorld()->FindFirst(
                                CGameWorld::ENTTYPE_CHARACTER));
                        for (; pC; pC = (CCharacter *) pC->TypeNext()) {
                            if (pC != pChr)
                                pC->SetEmote(EMOTE_SURPRISE, Server()->Tick() + Server()->TickSpeed());
                        }

                        pChr->SetEmote(EMOTE_ANGRY, Server()->Tick() + 1200 * Server()->TickSpeed() / 1000);
                    }
                    break;
                }

                default:
                    break;
            };

            if (Picked) {
                char aBuf[256];
                str_format(aBuf, sizeof(aBuf), "pickup player='%d:%s' item=%d map='%d",
                           pChr->GetPlayer()->GetCID(), Server()->ClientName(pChr->GetPlayer()->GetCID()), m_Type,
                           GetMapID());
                GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game/multimap", aBuf);
                int RespawnTime = g_pData->m_aPickups[m_Type].m_Respawntime;
                if (RespawnTime >= 0)
                    m_SpawnTick = Server()->Tick() + Server()->TickSpeed() * RespawnTime;
            }
        }
    }
}

void CPickup::TickPaused()
{
	if(m_SpawnTick != -1)
		++m_SpawnTick;
}

void CPickup::Snap(int SnappingClient)
{
    if(GameServer()->Server()->ClientMapID(SnappingClient) != GetMapID())
        return;

    if(m_Team!=-1) {
        if (GameServer()->GetClientTeam(SnappingClient)!=m_Team){
            return;
        }
    }

	if(GameServer()->Server()->ClientMapID(SnappingClient) != GetMapID())
		return;

	if(m_SpawnTick != -1 || NetworkClipped(SnappingClient))
		return;

	CNetObj_Pickup *pP = static_cast<CNetObj_Pickup *>(Server()->SnapNewItem(NETOBJTYPE_PICKUP, GetID(), sizeof(CNetObj_Pickup)));
	if(!pP)
		return;

	pP->m_X = round_to_int(m_Pos.x);
	pP->m_Y = round_to_int(m_Pos.y);
	pP->m_Type = m_Type;
}
