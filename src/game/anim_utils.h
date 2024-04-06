#pragma once

class floating_health_interface{
public:
    floating_health_interface(int max_hp, int mapId);
    void UpdateHealthInterface(int hp);
};