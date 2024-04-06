#include "player_classes.h"
#include "base/system.h"

int GetClassID(Class aClass){
    switch(aClass){
        case Class::None:
            return 0;
        case Class::Hunter:
            return 1;
        case Class::Medic:
            return 2;
        case Class::Scout:
            return 3;
        case Class::Tank:
            return 4;
        case Class::Spider:
            return 5;
        case Class::Engineer:
            return 6;
        case Class::Armorer:
            return 7;
        case Class::Necromancer:
            dbg_assert(false, "Class Necromancer is not implemented yet!");
            break;
    }
    return -1;//oop something went wrong!
}
Class GetClass(int ClassID){
    switch(ClassID){
        case 0:
            return Class::None;
        case 1:
            return Class::Hunter;
        case 2:
            return Class::Medic;
        case 3:
            return Class::Scout;
        case 4:
            return Class::Tank;
        case 5:
            return Class::Spider;
        case 6:
            return Class::Engineer;
        case 7:
            return Class::Armorer;
        case -1:
            dbg_assert(false, "Oop, something went wrong!");
            break;
    }
    dbg_assert(false, "Oop, something went wrong, but the wrong way!");
}