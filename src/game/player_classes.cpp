#include "player_classes.h"
#include "base/system.h"

CPlayerClass::CPlayerClass(int ClassID) {
    m_Class = GetClass(ClassID);
}

bool CPlayerClass::IsClass(int ClassID) const {
    return m_Class == GetClass(ClassID);
}

bool CPlayerClass::IsClass(Class Class) const {
    return m_Class == Class;
}

int GetClassID(Class Class) {
    return (int) Class;
}

Class GetClass(int ClassID) {
    dbg_assert(ClassID >= 0 && ClassID < (int) Class::class_count, "Oop, something went wrong, value out of range!");
    return (Class) ClassID;
}