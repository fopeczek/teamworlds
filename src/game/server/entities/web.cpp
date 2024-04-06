#include <generated/server_data.h>
#include "game/server/gamecontroller.h"
#include <game/server/gamecontext.h>
#include <numeric>
#include "game/server/entities/character.h"
#include "web.h"


CWeb::CWeb(CGameWorld *pGameWorld, int Owner, int MapID)
        : CEntity(pGameWorld, CGameWorld::ENTTYPE_LASER, vec2(0, 0), MapID) {
    m_Owner = Owner;
    m_EvalTick = 0;
    m_SpiderWebTick = 0;
    m_LastHitTick = 0;
    m_Hited = 0;
    pPlayer = GameServer()->m_apPlayers[Owner];
    m_Team = pPlayer->GetTeam();
    m_Fortified = false;
    GameWorld()->InsertEntity(this);
}

bool CWeb::CreateWeb(vec2 Dir) {
    if (pPlayer->GetCharacter()->m_aWeapons[pPlayer->GetCharacter()->m_ActiveWeapon].m_Ammo >= 1) {
        m_Dir = Dir;
        m_From = pPlayer->GetCharacter()->GetPos();
        m_Pos = m_From + m_Dir * m_laser_range;
        GameServer()->Collision(GetMapID())->IntersectLine(m_From, m_Pos, 0x0, &m_Pos);
        m_Pos = Clamp_vec(m_From, m_Pos, m_range);

        if (distance(m_Pos, m_From) >= radius * 2) {
            GameWorld()->InsertEntity(this);
            m_Delay_fac = 10000.0f;

            m_From = pPlayer->GetCharacter()->GetPos();
            m_From = Clamp_vec(m_Pos, m_From, m_laser_range);

            GameServer()->Collision(GetMapID())->IntersectLine(m_Pos, m_From, 0x0, &m_From);

            m_Health = m_MAX_Health;

            //setup hud interface
            if (str_comp(GameServer()->GameType(), "DM") != 0 and str_comp(GameServer()->GameType(), "LMS") != 0) {
                vec2 middle((m_Pos.x + m_From.x) / 2, (m_Pos.y + m_From.y) / 2);
                m_Hud_Interface[0] = new CPickup(GameWorld(), PICKUP_ARMOR, middle, GetMapID(), false,
                                                 m_Team);
                m_Hud_Interface[1] = new CPickup(GameWorld(), PICKUP_ARMOR, m_From, GetMapID(), false,
                                                 m_Team);
                m_Hud_Interface[2] = new CPickup(GameWorld(), PICKUP_ARMOR, m_Pos, GetMapID(), false,
                                                 m_Team);
            }

            m_SpiderWebTick = Server()->Tick();
            pPlayer->m_Spider_ActiveWebs++;
            if (!pPlayer->m_Cheats.AllWeapons) {
                pPlayer->GetCharacter()->m_aWeapons[WEAPON_SHOTGUN].m_Ammo -= 1;
            }
            GameServer()->CreateSound(m_Pos, SOUND_HOOK_NOATTACH, -1, GetMapID());
            return true;
        } else {
            return false;
        }
    } else {
        return false;
    }
}


void CWeb::Fortify() {
    if (!m_Fortified) {
        if (distance(m_Pos, m_From) >= radius * 2) {

            m_Health = m_MAX_Fortified_Health;

            //setup health animation
            vec2 pos;
            if (distance(diff, vec2(0, 0)) > 2 * radius) {
                pos = m_From;
            } else {
                midpoint1 = m_Pos;
                midpoint2 = m_From;
                vec2 vec = midpoint2 - midpoint1;
                theta = std::atan2(vec.x, vec.y) + pi / 2;
                diff = midpoint2 - midpoint1;
                versor = diff / distance(diff, vec2(0, 0));
                diff = midpoint2 - midpoint1 - versor * radius * 2;
                float line_segment_len = distance(diff, vec2(0, 0));
                total_path = radius * pi * 2 + 2 * line_segment_len;
                stops[0] = radius * pi / total_path;
                stops[1] = line_segment_len / total_path;
                stops[2] = radius * pi / total_path;
                stops[3] = line_segment_len / total_path;
                std::partial_sum(&stops[0], &stops[3], &cumsum_stops[0]);
                pos = Calc_hp_pos(m_HPTick / m_hp_interface_delay);
            }
            for (int i = 0; i < m_Health; i++) {
                if (pos != m_From) {
                    int HPTick = m_HPTick + i * m_hp_interface_space;
                    HPTick %= static_cast<int>(m_hp_interface_delay);
                    pos = Calc_hp_pos(HPTick / m_hp_interface_delay);
                }
                m_Health_Interface[i] = new CPickup(GameWorld(), PICKUP_HEALTH, pos, GetMapID(), false);
            }

            if (!pPlayer->m_Cheats.AllWeapons) {
                pPlayer->GetCharacter()->m_aWeapons[WEAPON_SHOTGUN].m_Ammo -= 1;
            }
            GameServer()->CreateSound(m_Pos, SOUND_PICKUP_HEALTH, -1, GetMapID());
            m_Fortified = true;
        }
    }
}

vec2 CWeb::Calc_hp_pos(float alpha) {
    if (alpha < cumsum_stops[0]) {
        alpha = alpha / stops[0];
        vec2 p;
        p.x = radius * sin(theta + alpha * pi);
        p.y = radius * cos(theta + alpha * pi);
        vec2 start = midpoint1 + versor * radius;
        return start + p;
    } else if (alpha < cumsum_stops[1]) {
        float a = (alpha - cumsum_stops[0]) / stops[1];

        vec2 start = midpoint1 + versor * radius +
                     vec2(-versor[1], versor[0]) * radius;
        vec2 end = midpoint2 - versor * radius +
                   vec2(-versor[1], versor[0]) * radius;
        vec2 p = start * (1 - a) + end * a;
        return p;
    } else if (alpha < cumsum_stops[2]) {
        alpha = (alpha - cumsum_stops[1]) / stops[2];
        vec2 p;
        p.x = radius * sin(theta - pi + alpha * pi);
        p.y = radius * cos(theta - pi + alpha * pi);
        vec2 start = midpoint2 - versor * radius;
        return start + p;
    } else {
        float a = (alpha - cumsum_stops[2]) / stops[3];

        vec2 start = midpoint2 - versor * radius +
                     vec2(versor[1], -versor[0]) * radius;
        vec2 end = midpoint1 + versor * radius +
                   vec2(versor[1], -versor[0]) * radius;
        vec2 p = start * (1 - a) + end * a;
        return p;
    }
}

vec2 CWeb::Clamp_vec(vec2 From, vec2 To, float clamp) {
    vec2 diff = To - From;
    float d = maximum(clamp, distance(To, From));
    vec2 clamped_end;
    if (d <= clamp) {
        clamped_end = To;
    } else {
        clamped_end = From + diff * clamp / d;
    }
    return clamped_end;
}

void CWeb::Reset() {
    //remove hp interface
    for (int i = 0; i < m_MAX_Health; i++) {
        if (m_Health_Interface[i]) {
            m_Health_Interface[i]->Destroy();
            m_Health_Interface[i] = nullptr;
        }
    }
    //remove hud interface
    for (int i = 0; i < 3; i++) {
        if (m_Hud_Interface[i]) {
            m_Hud_Interface[i]->Destroy();
            m_Hud_Interface[i] = nullptr;
        }
    }
    pPlayer->m_Spider_ActiveWebs--;
    pPlayer->m_Spider_ActiveWebs = clamp(pPlayer->m_Spider_ActiveWebs, 0, pPlayer->m_Spider_ActiveWebs);
    GameWorld()->DestroyEntity(this);
}

void CWeb::Tick() {
    if (GameServer()->GameTypeType()->m_GameState == GameServer()->GameTypeType()->EGameState::IGS_END_MATCH) {
        Reset();
    }
    if (Server()->Tick() >
        m_EvalTick + (Server()->TickSpeed() * GameServer()->Tuning()->m_LaserBounceDelay) / m_Delay_fac) {
        if (!m_Fortified) {
            if (m_SpiderWebTick + (Server()->TickSpeed() * 30) < Server()->Tick()) {
                Reset();
            }
        }
    }
}

void CWeb::TickPaused() {
    ++m_EvalTick;
    ++m_SpiderWebTick;
}

void CWeb::Snap(int SnappingClient) {
    if (GameServer()->Server()->ClientMapID(SnappingClient) != GetMapID())
        return;

    if (NetworkClipped(SnappingClient) && NetworkClipped(SnappingClient, m_From))
        return;

    CNetObj_Laser *pObj = static_cast<CNetObj_Laser *>(Server()->SnapNewItem(NETOBJTYPE_LASER, GetID(),
                                                                             sizeof(CNetObj_Laser)));
    if (!pObj)
        return;

    pObj->m_X = (int) m_Pos.x;
    pObj->m_Y = (int) m_Pos.y;
    pObj->m_FromX = (int) m_From.x;
    pObj->m_FromY = (int) m_From.y;
    pObj->m_StartTick = m_EvalTick;
}