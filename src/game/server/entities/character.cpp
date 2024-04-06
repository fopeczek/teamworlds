/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/shared/config.h>

#include <generated/server_data.h>
#include <game/server/gamecontext.h>
#include <game/server/gamecontroller.h>
#include <game/server/player.h>

#include "character.h"
#include "laser.h"
#include "projectile.h"
#include "game/player_classes.h"

//input count
struct CInputCount {
    int m_Presses;
    int m_Releases;
};

CInputCount CountInput(int Prev, int Cur) {
    CInputCount c = {0, 0};
    Prev &= INPUT_STATE_MASK;
    Cur &= INPUT_STATE_MASK;
    int i = Prev;

    while (i != Cur) {
        i = (i + 1) & INPUT_STATE_MASK;
        if (i & 1)
            c.m_Presses++;
        else
            c.m_Releases++;
    }

    return c;
}


MACRO_ALLOC_POOL_ID_IMPL(CCharacter, MAX_CLIENTS)

// Character, "physical" player's part
CCharacter::CCharacter(CGameWorld *pWorld, int MapID)
        : CEntity(pWorld, CGameWorld::ENTTYPE_CHARACTER, vec2(0, 0), MapID, ms_PhysSize) {
    m_Health = 0;
    m_Armor = 0;
    m_TriggeredEvents = 0;
}

void CCharacter::Reset() {
    Destroy();
}

bool CCharacter::Spawn(CPlayer *pPlayer, vec2 Pos) {
    m_EmoteStop = -1;
    m_LastAction = -1;
    m_LastNoAmmoSound = -1;
    m_ActiveWeapon = WEAPON_GUN;
    m_LastWeapon = WEAPON_HAMMER;
    m_QueuedWeapon = -1;

    m_pPlayer = pPlayer;
    m_Pos = Pos;

    m_Core.Reset();
    m_Core.Init(&GameWorld()->m_Core, GameServer()->Collision(GetMapID()), pPlayer->GetTeam(), GetMapID(),
                Server()->GetClientClass(m_pPlayer->GetCID()));
    m_Core.m_Pos = m_Pos;
    GameWorld()->m_Core.m_apCharacters[m_pPlayer->GetCID()] = &m_Core;

    m_ReckoningTick = 0;
    mem_zero(&m_SendCore, sizeof(m_SendCore));
    mem_zero(&m_ReckoningCore, sizeof(m_ReckoningCore));

    GameWorld()->InsertEntity(this);
    m_Alive = true;

    GameServer()->m_pController->OnCharacterSpawn(this);

    if (Server()->GetClientClass(GetPlayer()->GetCID()).IsClass(Class::Spider)) {
        m_ActiveWall = new CWall(GameWorld(), m_pPlayer->GetCID(), Server()->MainMapID, true);
    } else {
        m_ActiveWall = new CWall(GameWorld(), m_pPlayer->GetCID(), Server()->MainMapID);
    }

    for (int i = 0; i < MAX_PLAYERS; i++) {
        m_SpiderSenseHud[i] = nullptr;
        m_SpiderSenseCID[i] = -1;
    }

    return true;
}

void CCharacter::Destroy() {
    GameWorld()->m_Core.m_apCharacters[m_pPlayer->GetCID()] = 0;
    m_Alive = false;
}

void CCharacter::SetWeapon(int W) {
    if (W == m_ActiveWeapon)
        return;

    m_LastWeapon = m_ActiveWeapon;
    m_QueuedWeapon = -1;
    m_ActiveWeapon = W;
    GameServer()->CreateSound(m_Pos, SOUND_WEAPON_SWITCH, -1, GetMapID());

    if (m_ActiveWeapon < 0 || m_ActiveWeapon >= NUM_WEAPONS)
        m_ActiveWeapon = 0;
    m_aWeapons[m_ActiveWeapon].m_AmmoRegenStart = -1;
}

bool CCharacter::IsGrounded() {
    if (GameServer()->Collision(GetMapID())->CheckPoint(m_Pos.x + GetProximityRadius() / 2,
                                                        m_Pos.y + GetProximityRadius() / 2 + 5))
        return true;
    if (GameServer()->Collision(GetMapID())->CheckPoint(m_Pos.x - GetProximityRadius() / 2,
                                                        m_Pos.y + GetProximityRadius() / 2 + 5))
        return true;
    return false;
}


void CCharacter::HandleNinja() {
    if (m_ActiveWeapon != WEAPON_NINJA)
        return;

    if ((Server()->Tick() - m_Ninja.m_ActivationTick) >
        (g_pData->m_Weapons.m_Ninja.m_Duration * Server()->TickSpeed() / 1000)) {
        // time's up, return
        m_aWeapons[WEAPON_NINJA].m_Got = false;
        m_ActiveWeapon = m_LastWeapon;

        // reset velocity and current move
        if (m_Ninja.m_CurrentMoveTime > 0)
            m_Core.m_Vel = m_Ninja.m_ActivationDir * m_Ninja.m_OldVelAmount;
        m_Ninja.m_CurrentMoveTime = -1;

        SetWeapon(m_ActiveWeapon);
        return;
    }

    // force ninja Weapon
    SetWeapon(WEAPON_NINJA);

    m_Ninja.m_CurrentMoveTime--;

    if (m_Ninja.m_CurrentMoveTime == 0) {
        // reset velocity
        m_Core.m_Vel = m_Ninja.m_ActivationDir * m_Ninja.m_OldVelAmount;
    } else if (m_Ninja.m_CurrentMoveTime > 0) {
        // Set velocity
        m_Core.m_Vel = m_Ninja.m_ActivationDir * g_pData->m_Weapons.m_Ninja.m_Velocity;
        vec2 OldPos = m_Pos;
        GameServer()->Collision(GetMapID())->MoveBox(&m_Core.m_Pos, &m_Core.m_Vel,
                                                     vec2(GetProximityRadius(), GetProximityRadius()), 0.f);

        // reset velocity so the client doesn't predict stuff
        m_Core.m_Vel = vec2(0.f, 0.f);

        // check if we hit anything along the way
        const float Radius = GetProximityRadius() * 2.0f;
        const vec2 Center = OldPos + (m_Pos - OldPos) * 0.5f;
        CCharacter *aEnts[MAX_CLIENTS];
        const int Num = GameWorld()->FindEntities(Center, Radius, (CEntity **) aEnts, MAX_CLIENTS,
                                                  CGameWorld::ENTTYPE_CHARACTER, GetMapID());

        for (int i = 0; i < Num; ++i) {
            if (aEnts[i] == this)
                continue;

            // make sure we haven't hit this object before
            bool AlreadyHit = false;
            for (int j = 0; j < m_NumObjectsHit; j++) {
                if (m_apHitObjects[j] == aEnts[i]) {
                    AlreadyHit = true;
                    break;
                }
            }
            if (AlreadyHit)
                continue;

            // check so we are sufficiently close
            if (distance(aEnts[i]->m_Pos, m_Pos) > Radius)
                continue;

            // Hit a player, give him damage and stuffs...
            GameServer()->CreateSound(aEnts[i]->m_Pos, SOUND_NINJA_HIT, -1, GetMapID());
            if (m_NumObjectsHit < MAX_PLAYERS)
                m_apHitObjects[m_NumObjectsHit++] = aEnts[i];

            // set his velocity to fast upward (for now)
            aEnts[i]->TakeDamage(vec2(0, -10.0f), m_Ninja.m_ActivationDir * -1,
                                 g_pData->m_Weapons.m_Ninja.m_pBase->m_Damage, m_pPlayer->GetCID(), WEAPON_NINJA);
        }
    }
}


void CCharacter::DoWeaponSwitch() {
    // make sure we can switch
    if (m_ReloadTimer != 0 || m_QueuedWeapon == -1 || m_aWeapons[WEAPON_NINJA].m_Got)
        return;

    // switch Weapon
    SetWeapon(m_QueuedWeapon);
}

void CCharacter::HandleWeaponSwitch() {
    int WantedWeapon = m_ActiveWeapon;
    if (m_QueuedWeapon != -1)
        WantedWeapon = m_QueuedWeapon;

    // select Weapon
    int Next = CountInput(m_LatestPrevInput.m_NextWeapon, m_LatestInput.m_NextWeapon).m_Presses;
    int Prev = CountInput(m_LatestPrevInput.m_PrevWeapon, m_LatestInput.m_PrevWeapon).m_Presses;

    if (Next < 128) // make sure we only try sane stuff
    {
        while (Next) // Next Weapon selection
        {
            WantedWeapon = (WantedWeapon + 1) % NUM_WEAPONS;
            if (m_aWeapons[WantedWeapon].m_Got)
                Next--;
        }
    }

    if (Prev < 128) // make sure we only try sane stuff
    {
        while (Prev) // Prev Weapon selection
        {
            WantedWeapon = (WantedWeapon - 1) < 0 ? NUM_WEAPONS - 1 : WantedWeapon - 1;
            if (m_aWeapons[WantedWeapon].m_Got)
                Prev--;
        }
    }

    // Direct Weapon selection
    if (m_LatestInput.m_WantedWeapon)
        WantedWeapon = m_Input.m_WantedWeapon - 1;

    // check for insane values
    if (WantedWeapon >= 0 && WantedWeapon < NUM_WEAPONS && WantedWeapon != m_ActiveWeapon &&
        m_aWeapons[WantedWeapon].m_Got)
        m_QueuedWeapon = WantedWeapon;

    DoWeaponSwitch();
}

void CCharacter::FireWeapon() {
    if (m_pPlayer->m_Cheats.FullAuto and m_ActiveWeapon != WEAPON_NINJA) {
        m_ReloadTimer = 0;
    }

    if (m_ReloadTimer != 0) {
        return;
    }

    DoWeaponSwitch();
    vec2 Direction = normalize(vec2(m_LatestInput.m_TargetX, m_LatestInput.m_TargetY));

    bool FullAuto = false;
    if (m_ActiveWeapon == WEAPON_GRENADE || m_ActiveWeapon == WEAPON_SHOTGUN || m_ActiveWeapon == WEAPON_LASER)
        FullAuto = true;

    if (m_pPlayer->m_Cheats.FullAuto or (m_pPlayer->m_Cheats.SuperNinja and m_ActiveWeapon == WEAPON_NINJA)) {
        FullAuto = true;
    }
    if (Server()->GetClientClass(GetPlayer()->GetCID()).IsClass(Class::Engineer) and m_ActiveWeapon == WEAPON_LASER) {
        FullAuto = false;
    }
    if (Server()->GetClientClass(GetPlayer()->GetCID()).IsClass(Class::Tank) and m_ActiveWeapon == WEAPON_GUN) {
        FullAuto = true;
    }


    // check if we gonna fire
    bool WillFire = false;
    if (CountInput(m_LatestPrevInput.m_Fire, m_LatestInput.m_Fire).m_Presses)
        WillFire = true;

    if (FullAuto && (m_LatestInput.m_Fire & 1) && m_aWeapons[m_ActiveWeapon].m_Ammo)
        WillFire = true;

    if (!WillFire)
        return;

    // check for ammo
    if (!m_aWeapons[m_ActiveWeapon].m_Ammo) {
        // 125ms is a magical limit of how fast a human can click
        m_ReloadTimer = 125 * Server()->TickSpeed() / 1000;
        if (m_LastNoAmmoSound + Server()->TickSpeed() <= Server()->Tick()) {
            GameServer()->CreateSound(m_Pos, SOUND_WEAPON_NOAMMO, -1, GetMapID());
            m_LastNoAmmoSound = Server()->Tick();
        }
        return;
    }

    vec2 ProjStartPos = m_Pos + Direction * GetProximityRadius() * 0.75f;

    if (Config()->m_Debug) {
        char aBuf[256];
        str_format(aBuf, sizeof(aBuf), "shot player='%d:%s' team=%d weapon=%d", m_pPlayer->GetCID(),
                   Server()->ClientName(m_pPlayer->GetCID()), m_pPlayer->GetTeam(), m_ActiveWeapon);
        GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
    }

    switch (m_ActiveWeapon) {
        case WEAPON_HAMMER: {
            GameServer()->CreateSound(m_Pos, SOUND_HAMMER_FIRE, -1, GetMapID());

            CWall *apWalls[MAX_PLAYERS * MAX_ACTIVE_ENGINEER_WALLS + MAX_PLAYERS * MAX_ACTIVE_SPIDER_WEBS];
            int manyWalls = GameWorld()->FindEntities(ProjStartPos, 10000000000.f,
                                                      (CEntity **) apWalls,
                                                      MAX_PLAYERS * MAX_ACTIVE_ENGINEER_WALLS +
                                                      MAX_PLAYERS * MAX_ACTIVE_SPIDER_WEBS,
                                                      CGameWorld::ENTTYPE_LASER, GetMapID());

            for (int i = 0; i < manyWalls; ++i) {
                if (apWalls[i]) {
                    apWalls[i]->HeIsHealing(m_pPlayer);
                    apWalls[i]->HammerHit(g_pData->m_Weapons.m_Hammer.m_pBase->m_Damage, m_pPlayer);
                }
            }

            CCharacter *apEnts[MAX_CLIENTS];
            int Hits = 0;
            int Num = GameWorld()->FindEntities(ProjStartPos, GetProximityRadius() * 0.5f, (CEntity **) apEnts,
                                                MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER, GetMapID());

            for (int i = 0; i < Num; ++i) {
                CCharacter *pTarget = apEnts[i];

                if ((pTarget == this) ||
                    GameServer()->Collision(GetMapID())->IntersectLine(ProjStartPos, pTarget->m_Pos, NULL, NULL))
                    continue;

                if (pTarget->m_ShadowDimension) {
                    pTarget->RevealHunter(true);
                }

                // set his velocity to fast upward (for now)
                if (length(pTarget->m_Pos - ProjStartPos) > 0.0f)
                    GameServer()->CreateHammerHit(
                            pTarget->m_Pos - normalize(pTarget->m_Pos - ProjStartPos) * GetProximityRadius() * 0.5f,
                            GetMapID());
                else
                    GameServer()->CreateHammerHit(ProjStartPos, GetMapID());

                vec2 Dir;
                if (length(pTarget->m_Pos - m_Pos) > 0.0f)
                    Dir = normalize(pTarget->m_Pos - m_Pos);
                else
                    Dir = vec2(0.f, -1.f);

                pTarget->TakeDamage(vec2(0.f, -1.f) + normalize(Dir + vec2(0.f, -1.1f)) * 10.0f, Dir * -1,
                                    g_pData->m_Weapons.m_Hammer.m_pBase->m_Damage,
                                    m_pPlayer->GetCID(), m_ActiveWeapon);
                Hits++;
            }

            // if we Hit anything, we have to wait for the reload
            if (Hits)
                m_ReloadTimer = Server()->TickSpeed() / 3;

        }
            break;

        case WEAPON_GUN: {
            new CProjectile(GameWorld(), WEAPON_GUN,
                            m_pPlayer->GetCID(),
                            ProjStartPos,
                            Direction,
                            (int) (Server()->TickSpeed() * GameServer()->Tuning()->m_GunLifetime),
                            g_pData->m_Weapons.m_Gun.m_pBase->m_Damage, false, 0, -1, WEAPON_GUN, GetMapID());

            GameServer()->CreateSound(m_Pos, SOUND_GUN_FIRE, -1, GetMapID());
            if (Server()->GetClientClass(m_pPlayer->GetCID()).IsClass(Class::Tank)) {
                m_Tank_PistolShot++;
            }
        }
            break;

        case WEAPON_SHOTGUN: {
            int ShotSpread = 2;

            if (Server()->GetClientClass(m_pPlayer->GetCID()).IsClass(Class::Spider)) {
                if (!m_ActiveWall->FirstTryToFortify(Direction, m_pPlayer->GetCID())) {
                    if (m_pPlayer->m_Spider_ActiveWebs < MAX_ACTIVE_SPIDER_WEBS or
                        m_pPlayer->m_Cheats.Godmode) {
                        if (m_ActiveWall->SpiderWeb(Direction)) {
                            m_ActiveWall = new CWall(GameWorld(), m_pPlayer->GetCID(), GetMapID(), true);
                            for (int i = -ShotSpread; i <= ShotSpread; ++i) {
                                //                  middle  | middle right   |   middle left       | right end   | left end
                                float Spreading[] = {0, 0.070f * 3.5f, -0.070f * 3.5f, 0.185f * 3.5f,
                                                     -0.185f * 3.5f};
                                float a = angle(Direction);
                                a += Spreading[i + 2];
                                float v = 1 - (absolute(i) / (float) ShotSpread);
                                float Speed = mix((float) GameServer()->Tuning()->m_ShotgunSpeeddiff, 1.0f, v);

                                if (i != -2) {
                                    if (m_pPlayer->m_Spider_ActiveWebs < MAX_ACTIVE_SPIDER_WEBS or
                                        m_pPlayer->m_Cheats.Godmode) {
                                        if (m_ActiveWall->SpiderWeb(vec2(cosf(a), sinf(a)) * Speed)) {
                                            m_ActiveWall = new CWall(GameWorld(), m_pPlayer->GetCID(), GetMapID(),
                                                                     true);
                                        }
                                    }
                                }
                            }
                        } else {
                            GameServer()->CreateSound(m_Pos, SOUND_WEAPON_NOAMMO, -1, GetMapID());
                        }
                    } else {
                        GameServer()->CreateSound(m_Pos, SOUND_WEAPON_NOAMMO, -1, GetMapID());
                    }
                }
            } else if (!Server()->GetClientClass(m_pPlayer->GetCID()).IsClass(Class::Spider)) {

                for (int i = -ShotSpread; i <= ShotSpread; ++i) {
                    float Spreading[] = {-0.185f, -0.070f, 0, 0.070f, 0.185f};
                    float a = angle(Direction);
                    a += Spreading[i + 2];
                    float v = 1 - (absolute(i) / (float) ShotSpread);
                    float Speed = mix((float) GameServer()->Tuning()->m_ShotgunSpeeddiff, 1.0f, v);
                    new CProjectile(GameWorld(), WEAPON_SHOTGUN,
                                    m_pPlayer->GetCID(),
                                    ProjStartPos,
                                    vec2(cosf(a), sinf(a)) * Speed,
                                    (int) (Server()->TickSpeed() * GameServer()->Tuning()->m_ShotgunLifetime),
                                    g_pData->m_Weapons.m_Shotgun.m_pBase->m_Damage, false, 0, -1, WEAPON_SHOTGUN,
                                    GetMapID());
                }

                GameServer()->CreateSound(m_Pos, SOUND_SHOTGUN_FIRE, -1, GetMapID());
            }
        }
            break;

        case WEAPON_GRENADE: {
            if (Server()->GetClientClass(GetPlayer()->GetCID()).IsClass(Class::Scout)) {
                new CProjectile(GameWorld(), WEAPON_GRENADE,
                                m_pPlayer->GetCID(),
                                ProjStartPos,
                                Direction,
                                (int) (Server()->TickSpeed() * GameServer()->Tuning()->m_GrenadeLifetime),
                                g_pData->m_Weapons.m_Grenade.m_pBase->m_Damage / 2, true, 1.5f, SOUND_GRENADE_EXPLODE,
                                WEAPON_GRENADE, GetMapID());
            } else {
                new CProjectile(GameWorld(), WEAPON_GRENADE,
                                m_pPlayer->GetCID(),
                                ProjStartPos,
                                Direction,
                                (int) (Server()->TickSpeed() * GameServer()->Tuning()->m_GrenadeLifetime),
                                g_pData->m_Weapons.m_Grenade.m_pBase->m_Damage, true, 0, SOUND_GRENADE_EXPLODE,
                                WEAPON_GRENADE, GetMapID());
            }

            GameServer()->CreateSound(m_Pos, SOUND_GRENADE_FIRE, -1, GetMapID());
        }
            break;

        case WEAPON_LASER: {
            if (Server()->GetClientClass(GetPlayer()->GetCID()).IsClass(Class::Engineer)) {
                if (m_pPlayer->m_Engineer_ActiveWalls < MAX_ACTIVE_ENGINEER_WALLS or m_pPlayer->m_Cheats.Godmode) {
                    if (m_pPlayer->m_Engineer_Wall_Editing) {
                        int amm = m_aWeapons[m_ActiveWeapon].m_Ammo;
                        if (m_aWeapons[m_ActiveWeapon].m_Ammo > 4) {
                            amm = 5;
                        } else {
                            amm = m_aWeapons[m_ActiveWeapon].m_Ammo;
                        }
                        if (m_ActiveWall->EndWallEdit(amm)) {
                            m_pPlayer->m_Engineer_ActiveWalls++;
                            m_pPlayer->m_Engineer_Wall_Editing = false;
                            if (!m_pPlayer->m_Cheats.AllWeapons) {
                                m_aWeapons[m_ActiveWeapon].m_Ammo -= amm;
                            }
                            m_ActiveWall = new CWall(GameWorld(), m_pPlayer->GetCID(), GetMapID());
                        } else {
                            GameServer()->CreateSound(m_Pos, SOUND_WEAPON_NOAMMO, -1, GetMapID());
                            return;
                        }
                    } else {
                        if (!m_ActiveWall->Created) {
                            m_ActiveWall->StartWallEdit(Direction);
                            m_pPlayer->m_Engineer_Wall_Editing = true;
                        }
                    }
                    GameServer()->CreateSound(m_Pos, SOUND_LASER_FIRE, -1, GetMapID());
                } else {
                    GameServer()->CreateSound(m_Pos, SOUND_WEAPON_NOAMMO, -1, GetMapID());
                    return;
                }
            } else {
                new CLaser(GameWorld(), m_Pos, Direction, GameServer()->Tuning()->m_LaserReach, m_pPlayer->GetCID(),
                           GetMapID());
                GameServer()->CreateSound(m_Pos, SOUND_LASER_FIRE, -1, GetMapID());
            }
        }
            break;

        case WEAPON_NINJA: {
            m_NumObjectsHit = 0;

            m_Ninja.m_ActivationDir = Direction;
            m_Ninja.m_CurrentMoveTime = g_pData->m_Weapons.m_Ninja.m_Movetime * Server()->TickSpeed() / 1000;
            m_Ninja.m_OldVelAmount = length(m_Core.m_Vel);

            if (Server()->GetClientClass(m_pPlayer->GetCID()).IsClass(Class::Hunter)) {
                m_Ninja.m_CurrentMoveTime = -1;
                if (m_ShadowDimension) {
                    RevealHunter(false);
                } else {
                    if ((m_ShadowDimensionCooldown and m_Ninja.m_ActivationTick >= Server()->Tick()) or
                        !m_ShadowDimensionCooldown) {
                        bool too_close = false;
                        bool hooked = false;
                        for (int i = 0; i < MAX_PLAYERS; ++i) {
                            if (i != GetPlayer()->GetCID()) {
                                if (GameServer()->m_apPlayers[i]) {
                                    if (GameServer()->m_apPlayers[i]->GetCharacter()) {
                                        if (GameServer()->m_apPlayers[i]->GetTeam() != m_pPlayer->GetTeam()) {
                                            if (distance(GameServer()->m_apPlayers[i]->GetCharacter()->GetPos(),
                                                         m_Pos) <=
                                                400.f) {
                                                too_close = true;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        if (m_Core.m_HookState == HOOK_GRABBED) {
                            if (m_Core.m_HookedPlayer) {
                                if (Server()->ClientIngame(m_Core.m_HookedPlayer)) {
                                    if (GameServer()->GetClientTeam(m_Core.m_HookedPlayer) != m_pPlayer->GetTeam()) {
                                        hooked = true;
                                    }
                                }
                            }
                        }
                        if (hooked or too_close) {
                            GameServer()->CreateSound(m_Pos, SOUND_WEAPON_NOAMMO, -1, GetMapID());
                        } else {
                            HideHunter();
                        }
                    } else {
                        GameServer()->CreateSound(m_Pos, SOUND_WEAPON_NOAMMO, -1, GetMapID());
                    }
                }
            } else {
                GameServer()->CreateSound(m_Pos, SOUND_NINJA_FIRE, -1, GetMapID());
            }
        }
            break;

    }

    m_AttackTick = Server()->Tick();

    if (m_aWeapons[m_ActiveWeapon].m_Ammo > 0) { // -1 == unlimited
        bool take_ammo = true;
        if (Server()->GetClientClass(GetPlayer()->GetCID()).IsClass(Class::Engineer)) {
            if (m_ActiveWeapon == WEAPON_LASER) {
                take_ammo = false;
            }
        } else if (Server()->GetClientClass(GetPlayer()->GetCID()).IsClass(Class::Spider)) {
            if (m_ActiveWeapon == WEAPON_SHOTGUN) {
                take_ammo = false;
            }
        }
        if (m_pPlayer->m_Cheats.AllWeapons) {
            take_ammo = false;
        }
        if (Server()->GetClientClass(GetPlayer()->GetCID()).IsClass(Class::Tank)) {
            if (m_ActiveWeapon == WEAPON_GUN) {
                if (m_Tank_PistolShot == 3 and !m_pPlayer->m_Cheats.AllWeapons) {
                    m_aWeapons[m_ActiveWeapon].m_Ammo--;
                    m_Tank_PistolShot = 0;
                }
                take_ammo = false;
            }
        }
        if (take_ammo) {
            m_aWeapons[m_ActiveWeapon].m_Ammo--;
        }
    }

    if (!m_ReloadTimer) {
        m_ReloadTimer = g_pData->m_Weapons.m_aId[m_ActiveWeapon].m_Firedelay * Server()->TickSpeed() / 1000;
        if (Server()->GetClientClass(GetPlayer()->GetCID()).IsClass(Class::Tank) and m_ActiveWeapon == WEAPON_GUN) {
            m_ReloadTimer = g_pData->m_Weapons.m_aId[m_ActiveWeapon].m_Firedelay * Server()->TickSpeed() / 1000 - 2;
        }
        if (m_pPlayer->m_Cheats.SuperNinja and m_ActiveWeapon == WEAPON_NINJA) {
            m_ReloadTimer = 300.f * Server()->TickSpeed() / 1000;
        }
    }
}

void CCharacter::HandleWeapons() {
    //ninja
    HandleNinja();

    // check reload timer
    if (m_ReloadTimer) {
        m_ReloadTimer--;
        return;
    }

    // fire Weapon, if wanted
    FireWeapon();

    // ammo regen
    int AmmoRegenTime = g_pData->m_Weapons.m_aId[m_ActiveWeapon].m_Ammoregentime;
    if (AmmoRegenTime && m_aWeapons[m_ActiveWeapon].m_Ammo >= 0) {
        // If equipped and not active, regen ammo?
        if (m_ReloadTimer <= 0) {
            if (m_aWeapons[m_ActiveWeapon].m_AmmoRegenStart < 0)
                m_aWeapons[m_ActiveWeapon].m_AmmoRegenStart = Server()->Tick();

            if ((Server()->Tick() - m_aWeapons[m_ActiveWeapon].m_AmmoRegenStart) >=
                AmmoRegenTime * Server()->TickSpeed() / 1000) {
                // Add some ammo
                m_aWeapons[m_ActiveWeapon].m_Ammo = minimum(m_aWeapons[m_ActiveWeapon].m_Ammo + 1,
                                                            g_pData->m_Weapons.m_aId[m_ActiveWeapon].m_Maxammo);
                m_aWeapons[m_ActiveWeapon].m_AmmoRegenStart = -1;
            }
        } else {
            m_aWeapons[m_ActiveWeapon].m_AmmoRegenStart = -1;
        }
    }

    return;
}

bool CCharacter::GiveWeapon(int Weapon, int Ammo) {
    if (m_aWeapons[Weapon].m_Ammo < g_pData->m_Weapons.m_aId[Weapon].m_Maxammo || !m_aWeapons[Weapon].m_Got) {
        m_aWeapons[Weapon].m_Got = true;
        m_aWeapons[Weapon].m_Ammo = minimum(g_pData->m_Weapons.m_aId[Weapon].m_Maxammo, Ammo);
        return true;
    }
    return false;
}

void CCharacter::GiveNinja() {
    m_Ninja.m_ActivationTick = Server()->Tick();
    m_aWeapons[WEAPON_NINJA].m_Got = true;
    m_aWeapons[WEAPON_NINJA].m_Ammo = -1;
    if (m_ActiveWeapon != WEAPON_NINJA)
        m_LastWeapon = m_ActiveWeapon;
    m_ActiveWeapon = WEAPON_NINJA;

    GameServer()->CreateSound(m_Pos, SOUND_PICKUP_NINJA, -1, GetMapID());
}

void CCharacter::LoseNinja() {
    m_aWeapons[WEAPON_NINJA].m_Got = false;
    m_ActiveWeapon = m_LastWeapon;

    // reset velocity and current move
    if (m_Ninja.m_CurrentMoveTime > 0)
        m_Core.m_Vel = m_Ninja.m_ActivationDir * m_Ninja.m_OldVelAmount;
    m_Ninja.m_CurrentMoveTime = -1;

    SetWeapon(m_ActiveWeapon);
}

void CCharacter::SetEmote(int Emote, int Tick) {
    m_EmoteType = Emote;
    m_EmoteStop = Tick;
}

void CCharacter::OnPredictedInput(CNetObj_PlayerInput *pNewInput) {
    // check for changes
    if (mem_comp(&m_Input, pNewInput, sizeof(CNetObj_PlayerInput)) != 0)
        m_LastAction = Server()->Tick();

    // copy new input
    mem_copy(&m_Input, pNewInput, sizeof(m_Input));
    m_NumInputs++;

    // it is not allowed to aim in the center
    if (m_Input.m_TargetX == 0 && m_Input.m_TargetY == 0)
        m_Input.m_TargetY = -1;
}

void CCharacter::OnDirectInput(CNetObj_PlayerInput *pNewInput) {
    mem_copy(&m_LatestPrevInput, &m_LatestInput, sizeof(m_LatestInput));
    mem_copy(&m_LatestInput, pNewInput, sizeof(m_LatestInput));

    // it is not allowed to aim in the center
    if (m_LatestInput.m_TargetX == 0 && m_LatestInput.m_TargetY == 0)
        m_LatestInput.m_TargetY = -1;

    if (m_NumInputs > 2 && m_pPlayer->GetTeam() != TEAM_SPECTATORS and
        !Server()->GetClientClass(GetPlayer()->GetCID()).IsClass(Class::None) and GetMapID() != Server()->LobbyMapID) {
        if (!m_pPlayer->m_Cheats.LockWeapons) {
            HandleWeaponSwitch();
            FireWeapon();
        }
    }

    mem_copy(&m_LatestPrevInput, &m_LatestInput, sizeof(m_LatestInput));
}

void CCharacter::ResetInput() {
    m_Input.m_Direction = 0;
    m_Input.m_Hook = 0;
    // simulate releasing the fire button
    if ((m_Input.m_Fire & 1) != 0)
        m_Input.m_Fire++;
    m_Input.m_Fire &= INPUT_STATE_MASK;
    m_Input.m_Jump = 0;
    m_LatestPrevInput = m_LatestInput = m_Input;
}

void CCharacter::Teleport(vec2 where) {
    m_Pos = where;
    m_Core.m_Pos = where;
    m_Core.m_Vel = vec2(0, 0);
    Tick();
}

void CCharacter::RevealHunter(bool Cooldown) {
    if (!m_pPlayer->m_Cheats.Godmode) {
        m_ShadowDimension = false;
        m_ShadowDimensionCooldown = Cooldown;
        GameServer()->CreateSound(m_Pos, SOUND_PICKUP_NINJA, -1, GetMapID());
    }
}

void CCharacter::HideHunter() {
    if (!m_pPlayer->m_Cheats.Godmode) {
        m_ShadowDimension = true;
        m_ShadowDimensionCooldown = false;
        m_Ninja.m_ActivationTick = clamp(m_Ninja.m_ActivationTick, 0, Server()->Tick());
        GameServer()->CreateSound(m_Pos, SOUND_NINJA_FIRE, -1, GetMapID());
    }
}

void CCharacter::AddSpiderSenseHud(CCharacter *pChar) {
    if (Server()->GetClientClass(m_pPlayer->GetCID()).IsClass(Class::Spider)) {
        vec2 Dir(pChar->GetPos().x - m_Pos.x, pChar->GetPos().y - m_Pos.y);
        const float R = length(Dir);
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (m_SpiderSenseCID[i] == pChar->m_pPlayer->GetCID()) {
                return;
            }
            if (!m_SpiderSenseHud[i]) {
                m_SpiderSenseHud[i] = new CPickup(GameWorld(), PICKUP_HEALTH,
                                                  m_Pos + Dir / R * 100.f,
                                                  GetMapID(), false, m_pPlayer->m_Team);
                m_SpiderSenseTick[i] = Server()->Tick();
                m_SpiderSenseCID[i] = pChar->m_pPlayer->GetCID();
                return;
            }
        }
    }
}

void CCharacter::UpdateSpiderSenseHud() {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (m_SpiderSenseHud[i]) {
            if (Server()->Tick() < m_SpiderSenseTick[i] + 500.f) {
                if (GameServer()->GetPlayerChar(m_SpiderSenseCID[i])) {
                    if (distance(m_SpiderSenseHud[i]->GetPos(),
                                 GameServer()->GetPlayerChar(m_SpiderSenseCID[i])->GetPos()) >
                        MIN_SPIDER_SENSE_DISTANCE) {
                        vec2 Direction(GameServer()->GetPlayerChar(m_SpiderSenseCID[i])->GetPos().x - m_Pos.x,
                                       GameServer()->GetPlayerChar(m_SpiderSenseCID[i])->GetPos().y - m_Pos.y);
                        const float Distance = length(Direction);
                        const float Factor = Distance / m_SpiderSenseHudDistanceFactor;
                        vec2 Offset(Direction.x / Distance * Factor, Direction.y / Distance * Factor);
                        m_SpiderSenseHud[i]->SetPos(m_Pos + Offset);
                    } else {
                        if (m_SpiderSenseHud[i]) {
                            m_SpiderSenseHud[i]->Destroy();
                            m_SpiderSenseHud[i] = nullptr;
                            m_SpiderSenseCID[i] = -1;
                            m_SpiderSenseTick[i] = 0;
                        }
                    }
                } else {
                    if (m_SpiderSenseHud[i]) {
                        m_SpiderSenseHud[i]->Destroy();
                        m_SpiderSenseHud[i] = nullptr;
                        m_SpiderSenseCID[i] = -1;
                        m_SpiderSenseTick[i] = 0;
                    }
                }
            } else {
                if (!m_pPlayer->m_Cheats.Godmode) {
                    if (m_SpiderSenseHud[i]) {
                        m_SpiderSenseHud[i]->Destroy();
                        m_SpiderSenseHud[i] = nullptr;
                        m_SpiderSenseCID[i] = -1;
                        m_SpiderSenseTick[i] = 0;
                    }
                }
            }
        }
    }
}

void CCharacter::Tick() {
    if (m_Health < 10 and m_pPlayer->m_Cheats.Godmode) {
        m_Health = 10;
    }
    if (m_Armor < 10 and m_pPlayer->m_Cheats.Godmode) {
        m_Armor = 10;
    }
    if (m_pPlayer->m_Cheats.AllWeapons) {
        for (int i = 0; i < NUM_WEAPONS; i++) {
            if (i != WEAPON_NINJA) {
                if (!m_aWeapons[i].m_Got) {
                    m_aWeapons[i].m_Got = true;
                    m_aWeapons[i].m_Ammo = g_pData->m_Weapons.m_aId[i].m_Maxammo;
                } else if (m_aWeapons[i].m_Ammo < g_pData->m_Weapons.m_aId[i].m_Maxammo) {
                    m_aWeapons[i].m_Ammo = g_pData->m_Weapons.m_aId[i].m_Maxammo;
                }
            }
        }
    }
    if (m_pPlayer->m_Cheats.LockPosition) {
        m_Core.LockPos(m_pPlayer->m_Cheats.LockPos);
    }

    bool doReveal;
    m_Core.m_Input = m_Input;
    m_Core.Tick(true, doReveal, &m_pPlayer->m_Cheats);
    if (m_ShadowDimension and doReveal) {
        RevealHunter(true);
    }

    if (Server()->GetClientClass(m_pPlayer->GetCID()).IsClass(Class::Spider)) {
        UpdateSpiderSenseHud();
    }

    // handle leaving gamelayer
    if (GameLayerClipped(m_Pos)) {
        Die(m_pPlayer->GetCID(), WEAPON_WORLD);
    }

    // handle Weapons
    if (!m_pPlayer->m_Cheats.LockWeapons) {
        HandleWeapons();
    }
}

void CCharacter::TickDefered() {
    static const vec2 ColBox(CCharacterCore::PHYS_SIZE, CCharacterCore::PHYS_SIZE);
    // advance the dummy
    {
        CWorldCore TempWorld;
        m_ReckoningCore.Init(&TempWorld, GameServer()->Collision(GetMapID()), m_pPlayer->GetTeam(), GetMapID(),
                             Server()->GetClientClass(m_pPlayer->GetCID()));
        m_ReckoningCore.Tick(false);
        m_ReckoningCore.Move();
        m_ReckoningCore.Quantize();
    }

    // apply drag velocity when the player is not firing ninja
    // and set it back to 0 for the next tick
    if (m_ActiveWeapon != WEAPON_NINJA || m_Ninja.m_CurrentMoveTime < 0)
        m_Core.AddDragVelocity();
    m_Core.ResetDragVelocity();

    //lastsentcore
    vec2 StartPos = m_Core.m_Pos;
    vec2 StartVel = m_Core.m_Vel;
    bool StuckBefore = GameServer()->Collision(GetMapID())->TestBox(m_Core.m_Pos, ColBox);

    m_Core.Move();

    bool StuckAfterMove = GameServer()->Collision(GetMapID())->TestBox(m_Core.m_Pos, ColBox);
    m_Core.Quantize();
    bool StuckAfterQuant = GameServer()->Collision(GetMapID())->TestBox(m_Core.m_Pos, ColBox);
    m_Pos = m_Core.m_Pos;

    if (!StuckBefore && (StuckAfterMove || StuckAfterQuant)) {
        // Hackish solution to get rid of strict-aliasing warning
        union {
            float f;
            unsigned u;
        } StartPosX, StartPosY, StartVelX, StartVelY;

        StartPosX.f = StartPos.x;
        StartPosY.f = StartPos.y;
        StartVelX.f = StartVel.x;
        StartVelY.f = StartVel.y;

        char aBuf[256];
        str_format(aBuf, sizeof(aBuf), "STUCK!!! %d %d %d %f %f %f %f %x %x %x %x",
                   StuckBefore,
                   StuckAfterMove,
                   StuckAfterQuant,
                   StartPos.x, StartPos.y,
                   StartVel.x, StartVel.y,
                   StartPosX.u, StartPosY.u,
                   StartVelX.u, StartVelY.u);
        GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
    }

    m_TriggeredEvents |= m_Core.m_TriggeredEvents;

    if (m_pPlayer->GetTeam() == TEAM_SPECTATORS) {
        m_Pos.x = m_Input.m_TargetX;
        m_Pos.y = m_Input.m_TargetY;
    } else if (m_Core.m_Death) {
        // handle death-tiles
        Die(m_pPlayer->GetCID(), WEAPON_WORLD);
    }

    // update the m_SendCore if needed
    {
        CNetObj_Character Predicted;
        CNetObj_Character Current;
        mem_zero(&Predicted, sizeof(Predicted));
        mem_zero(&Current, sizeof(Current));
        m_ReckoningCore.Write(&Predicted);
        m_Core.Write(&Current);

        // only allow dead reackoning for a top of 3 seconds
        if (m_ReckoningTick + Server()->TickSpeed() * 3 < Server()->Tick() ||
            mem_comp(&Predicted, &Current, sizeof(CNetObj_Character)) != 0) {
            m_ReckoningTick = Server()->Tick();
            m_SendCore = m_Core;
            m_ReckoningCore = m_Core;
        }
    }
}

void CCharacter::TickPaused() {
    ++m_AttackTick;
    ++m_Ninja.m_ActivationTick;
    ++m_ReckoningTick;
    if (m_LastAction != -1)
        ++m_LastAction;
    if (m_aWeapons[m_ActiveWeapon].m_AmmoRegenStart > -1)
        ++m_aWeapons[m_ActiveWeapon].m_AmmoRegenStart;
    if (m_EmoteStop > -1)
        ++m_EmoteStop;
}

bool CCharacter::IncreaseHealth(int Amount) {
    if (m_Health >= 10)
        return false;
    m_Health = clamp(m_Health + Amount, 0, 10);
    return true;
}

bool CCharacter::IncreaseArmor(int Amount) {
    if (m_Armor >= 10)
        return false;
    m_Armor = clamp(m_Armor + Amount, 0, 10);
    return true;
}

void CCharacter::Die(int Killer, int Weapon) {
    if (!m_pPlayer->m_Cheats.Godmode) { //you can respawn without ing losing flag and score
        // we got to wait 0.5 secs before respawning
        m_Alive = false;
        m_pPlayer->m_RespawnTick = Server()->Tick() + Server()->TickSpeed() / 2;
        int ModeSpecial = GameServer()->m_pController->OnCharacterDeath(this, (Killer < 0) ? 0
                                                                                           : GameServer()->m_apPlayers[Killer],
                                                                        Weapon);

        char aBuf[256];
        if (Killer < 0) {
            str_format(aBuf, sizeof(aBuf), "kill killer='%d:%d:' victim='%d:%d:%s' weapon=%d special=%d",
                       Killer, -1 - Killer,
                       m_pPlayer->GetCID(), m_pPlayer->GetTeam(), Server()->ClientName(m_pPlayer->GetCID()), Weapon,
                       ModeSpecial
            );
        } else {
            str_format(aBuf, sizeof(aBuf), "kill killer='%d:%d:%s' victim='%d:%d:%s' weapon=%d special=%d",
                       Killer, GameServer()->m_apPlayers[Killer]->GetTeam(), Server()->ClientName(Killer),
                       m_pPlayer->GetCID(), m_pPlayer->GetTeam(), Server()->ClientName(m_pPlayer->GetCID()), Weapon,
                       ModeSpecial
            );
        }
        GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

        // send the kill message
        CNetMsg_Sv_KillMsg Msg;
        Msg.m_Victim = m_pPlayer->GetCID();
        Msg.m_ModeSpecial = ModeSpecial;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (!Server()->ClientIngame(i))
                continue;

            if (Killer < 0 && Server()->GetClientVersion(i) < MIN_KILLMESSAGE_CLIENTVERSION) {
                Msg.m_Killer = 0;
                Msg.m_Weapon = WEAPON_WORLD;
            } else {
                Msg.m_Killer = Killer;
                Msg.m_Weapon = Weapon;
            }
            Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, i);
        }

        // a nice sound
        GameServer()->CreateSound(m_Pos, SOUND_PLAYER_DIE, -1, GetMapID());

        // this is for auto respawn after 3 secs
        m_pPlayer->m_DieTick = Server()->Tick();

        GameWorld()->RemoveEntity(this);
        GameWorld()->m_Core.m_apCharacters[m_pPlayer->GetCID()] = 0;
        GameServer()->CreateDeath(m_Pos, m_pPlayer->GetCID(), GetMapID());
    }
}

bool CCharacter::TakeDamage(vec2 Force, vec2 Source, int Dmg, int From, int Weapon) {
    vec2 NewForce = Force;
    vec2 ScoutForce = vec2(0.f, 0.f);
    if (From != m_pPlayer->GetCID()) {
        if (Server()->GetClientClass(From).IsClass(Class::Scout) and Weapon == WEAPON_GRENADE) {
            // get ground state
            const bool Grounded =
                    m_Core.m_pCollision->CheckPoint(m_Pos.x + m_Core.PHYS_SIZE / 2,
                                                    m_Pos.y + m_Core.PHYS_SIZE / 2 + 5)
                    ||
                    m_Core.m_pCollision->CheckPoint(m_Pos.x - m_Core.PHYS_SIZE / 2,
                                                    m_Pos.y + m_Core.PHYS_SIZE / 2 + 5);

            ScoutForce = Force;
            if (Grounded) {
                if (Force.y < 0.f) {
                    ScoutForce.y -= 3.f;
                }
            }
        }
    }
    if (Server()->GetClientClass(m_pPlayer->GetCID()).IsClass(Class::Tank)) {
        if (Server()->GetClientClass(From).IsClass(Class::Scout) and Weapon == WEAPON_GRENADE) {
            ScoutForce /= 4.f;
        } else {
            NewForce = Force / 2.f;
        }
    }

    //all done, adding the force
    m_Core.m_Vel += NewForce + ScoutForce;

    if (From >= 0) {
        if (GameServer()->m_pController->IsFriendlyFire(m_pPlayer->GetCID(), From))
            return false;
    } else {
        int Team = TEAM_RED;
        if (From == PLAYER_TEAM_BLUE)
            Team = TEAM_BLUE;
        if (GameServer()->m_pController->IsFriendlyTeamFire(m_pPlayer->GetTeam(), Team))
            return false;
    }

    if (From == m_pPlayer->GetCID()) {
        if (Server()->GetClientClass(m_pPlayer->GetCID()).IsClass(Class::Scout)) {
            Dmg = 1;//scout deals 1 damage on self
        } else if (Server()->GetClientClass(m_pPlayer->GetCID()).IsClass(Class::Engineer)) {
            Dmg = 4;//engineer deals 4 damage on self
        } else {
            Dmg = maximum(1, Dmg / 2);// normal player only inflicts half damage on self
        }
        if (m_pPlayer->m_Cheats.NoSelfDamage) {
            Dmg = 0;
        }
    } else {
        if (m_pPlayer->m_Cheats.NoEnemyDamage) {
            Dmg = 0;
        }
    }

    if (Server()->GetClientClass(m_pPlayer->GetCID()).IsClass(Class::Tank)) {
        if (Dmg > 1) {
            Dmg = round_to_int(Dmg / 2.f);
        } else {
            if (m_Tank_PistolHitTick + 500 <= Server()->Tick()) { //after 5 sec reset pistol hit
                m_pPlayer->m_Tank_PistolHit = false;
            }
            if (m_pPlayer->m_Tank_PistolHit) {
                m_pPlayer->m_Tank_PistolHit = false;
            } else {
                Dmg = 0;
                m_pPlayer->m_Tank_PistolHit = true;
                m_Tank_PistolHitTick = Server()->Tick();
            }
        }
    }

    if (m_pPlayer->m_Cheats.Godmode) {
        Dmg = 0;
    }

    int OldHealth = m_Health, OldArmor = m_Armor;
    if (Dmg) {
        if (m_Armor) {
            if (Dmg > 1) {
                m_Health--;
                Dmg--;
            }

            if (Dmg > m_Armor) {
                Dmg -= m_Armor;
                m_Armor = 0;
            } else {
                m_Armor -= Dmg;
                Dmg = 0;
            }
        }

        m_Health -= Dmg;
    }

    // create healthmod indicator
    GameServer()->CreateDamage(m_Pos, m_pPlayer->GetCID(), Source, OldHealth - m_Health, OldArmor - m_Armor,
                               From == m_pPlayer->GetCID(), GetMapID());

    // do damage Hit sound
    if (From >= 0 && From != m_pPlayer->GetCID() && GameServer()->m_apPlayers[From]) {
        int64 Mask = CmaskOne(From);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (GameServer()->m_apPlayers[i] && (GameServer()->m_apPlayers[i]->GetTeam() == TEAM_SPECTATORS ||
                                                 GameServer()->m_apPlayers[i]->m_DeadSpecMode) &&
                GameServer()->m_apPlayers[i]->GetSpectatorID() == From)
                Mask |= CmaskOne(i);
        }
        GameServer()->CreateSound(GameServer()->m_apPlayers[From]->m_ViewPos, SOUND_HIT, Mask, GetMapID());
    }

    // check for death
    if (m_Health <= 0) {
        Die(From, Weapon);

        // set attacker's face to happy (taunt!)
        if (From >= 0 && From != m_pPlayer->GetCID() && GameServer()->m_apPlayers[From]) {
            CCharacter *pChr = GameServer()->m_apPlayers[From]->GetCharacter();
            if (pChr) {
                pChr->SetEmote(EMOTE_HAPPY, Server()->Tick() + Server()->TickSpeed());
            }
        }

        return false;
    }

    if (!Server()->GetClientSmile(m_pPlayer->GetCID())) {
        if (Dmg > 2)
            GameServer()->CreateSound(m_Pos, SOUND_PLAYER_PAIN_LONG, -1, GetMapID());
        else
            GameServer()->CreateSound(m_Pos, SOUND_PLAYER_PAIN_SHORT, -1, GetMapID());

        SetEmote(EMOTE_PAIN, Server()->Tick() + 500 * Server()->TickSpeed() / 1000);
    }

    return true;
}

void CCharacter::ConRemoveAllWalls() {
    CWall *allWalls[MAX_PLAYERS * MAX_ACTIVE_SPIDER_WEBS + MAX_PLAYERS * MAX_ACTIVE_ENGINEER_WALLS];
    int manyWalls = GameWorld()->FindEntities(GetPos(), 1000000000.f, (CEntity **) allWalls,
                                              MAX_PLAYERS * MAX_ACTIVE_SPIDER_WEBS +
                                              MAX_PLAYERS * MAX_ACTIVE_ENGINEER_WALLS, GameWorld()->ENTTYPE_LASER,
                                              GetMapID());
    if (manyWalls > 0) {
        for (int i = 0; i < manyWalls; i++) {
            if (allWalls[i]) {
                if (allWalls[i]->pPlayer) {
                    if (allWalls[i]->pPlayer->GetCharacter()) {
                        allWalls[i]->pPlayer->GetCharacter()->m_ActiveWall = new CWall(GameWorld(), m_pPlayer->GetCID(),
                                                                                       GetMapID());
                    }
                }
                if (allWalls[i]->m_Done or allWalls[i]->m_SpiderWeb) {
                    allWalls[i]->Die(-2);
                } else {
                    if (allWalls[i]->pPlayer) {
                        allWalls[i]->pPlayer->m_Engineer_Wall_Editing = false;
                    }
                    allWalls[i]->Destroy();
                }
            }
        }
    }
}

void CCharacter::Snap(int SnappingClient) {
    if (GameServer()->Server()->ClientMapID(SnappingClient) != GetMapID())
        return;

    if (SnappingClient != m_pPlayer->GetCID() and m_ShadowDimension and
        GameServer()->GetClientTeam(SnappingClient) != GetPlayer()->GetTeam()) {
        if (GameServer()->GetPlayerChar(SnappingClient)) {
            if (!GameServer()->GetPlayerChar(SnappingClient)->m_ShadowDimension) {
                return;
            }
        }
    }

    if (NetworkClipped(SnappingClient))
        return;

    CNetObj_Character *pCharacter = static_cast<CNetObj_Character *>(Server()->SnapNewItem(NETOBJTYPE_CHARACTER,
                                                                                           m_pPlayer->GetCID(),
                                                                                           sizeof(CNetObj_Character)));
    if (!pCharacter)
        return;

    // write down the m_Core
    if (!m_ReckoningTick || GameWorld()->m_Paused) {
        // no dead reckoning when paused because the client doesn't know
        // how far to perform the reckoning
        pCharacter->m_Tick = 0;
        m_Core.Write(pCharacter);
    } else {
        pCharacter->m_Tick = m_ReckoningTick;
        m_SendCore.Write(pCharacter);
    }

    // set emote
    if (m_EmoteStop < Server()->Tick()) {
        if (Server()->GetClientSmile(m_pPlayer->GetCID())) {
            SetEmote(EMOTE_HAPPY, -1);
        } else {
            SetEmote(EMOTE_NORMAL, -1);
        }
    }

    pCharacter->m_Emote = m_EmoteType;

    pCharacter->m_AmmoCount = 0;
    pCharacter->m_Health = 0;
    pCharacter->m_Armor = 0;
    pCharacter->m_TriggeredEvents = m_TriggeredEvents;

    pCharacter->m_Weapon = m_ActiveWeapon;
    pCharacter->m_AttackTick = m_AttackTick;

    pCharacter->m_Direction = m_Input.m_Direction;

    if (m_pPlayer->GetCID() == SnappingClient || SnappingClient == -1 ||
        (!Config()->m_SvStrictSpectateMode &&
         m_pPlayer->GetCID() == GameServer()->m_apPlayers[SnappingClient]->GetSpectatorID())) {
        pCharacter->m_Health = m_Health;
        pCharacter->m_Armor = m_Armor;
        if (m_ActiveWeapon == WEAPON_NINJA)
            pCharacter->m_AmmoCount =
                    m_Ninja.m_ActivationTick + g_pData->m_Weapons.m_Ninja.m_Duration * Server()->TickSpeed() / 1000;
        else if (m_aWeapons[m_ActiveWeapon].m_Ammo > 0)
            pCharacter->m_AmmoCount = m_aWeapons[m_ActiveWeapon].m_Ammo;
    }

    if (pCharacter->m_Emote == EMOTE_NORMAL) {
        if (5 * Server()->TickSpeed() - ((Server()->Tick() - m_LastAction) % (5 * Server()->TickSpeed())) < 5)
            pCharacter->m_Emote = EMOTE_BLINK;
    }
}

void CCharacter::PostSnap() {
    m_TriggeredEvents = 0;
}
