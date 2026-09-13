/* Generated verbatim upstream function extraction. See extracted.json. */
#include "sm64.h"
#include "behavior_data.h"
#include "game/object_list_processor.h"
#include "game/object_helpers.h"

#line 51 "n64decomp/src/game/spawn_sound.c"
void create_sound_spawner(s32 soundMagic) {
    struct Object *obj = spawn_object(gCurrentObject, 0, bhvSoundSpawner);

    obj->oSoundEffectUnkF4 = soundMagic;
}
