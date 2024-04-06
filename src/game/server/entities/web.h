#pragma once

#include "game/server/entity.h"
#include "game/server/player.h"
#include <game/server/gamecontext.h>
#include "pickup.h"

class CWeb : public CEntity {
public:
    CWeb(CGameWorld *pGameWorld, int Owner, int MapID);

    virtual void Reset();

    virtual void Tick();

    virtual void TickPaused();

    virtual void Snap(int SnappingClient);

    bool CreateWeb(vec2 Dir);

    void Fortify();

    bool TryToFortify(vec2 Dir, int CID);

    bool Intersect();

    int m_Owner;
    CPlayer *pPlayer;
private:
    vec2 Calc_hp_pos(float alpha);

    vec2 Clamp_vec(vec2 From, vec2 To, float clamp);

    int m_Team;

    static constexpr float m_laser_range = 800.f;
    static constexpr float m_deconstruct_range = 50.f;
    static constexpr float m_collision_range = 30.f;
    static constexpr float m_hp_interface_delay = 500.f;
    static constexpr float m_hp_interface_space = 50.f;
    static constexpr int m_MAX_Health = 1;
    static constexpr int m_MAX_Fortified_Health = 2;
    static constexpr float m_WebHitDelay = 5.f;
    static constexpr int m_MaxHits = 3;
    static constexpr float m_range = 500.f;
    static constexpr float m_max_speed = 2.5f;

    vec2 m_From;
    vec2 m_Dir;
    int m_EvalTick;
    int m_SpiderWebTick;
    int m_LastHitTick;
    int m_HPTick;

    bool m_Fortified;
    float m_Delay_fac;
    int m_Health;
    int m_Hited;
    bool m_WaitingToConfirm = false;
    int m_ConfirmTick;

    CPickup *m_Health_Interface[m_MAX_Health];
    CPickup *m_Hud_Interface[3];

    void UpdateHealthInterface();

    //-----------------------for calculations-------------------------------
    static constexpr float radius = 50.f;

    vec2 midpoint1;
    vec2 midpoint2;
    float theta;
    vec2 versor;
    vec2 diff;
    float total_path;
    float stops[4];
    float cumsum_stops[4];
};