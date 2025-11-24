#include "pch-collib-tests.h"

#include "life_cycle_object.h"

int LifeCycleObject::default_constructed = 0;
int LifeCycleObject::copy_constructed = 0;
int LifeCycleObject::move_constructed = 0;
int LifeCycleObject::copy_assigned = 0;
int LifeCycleObject::move_assigned = 0;
int LifeCycleObject::destructed = 0;