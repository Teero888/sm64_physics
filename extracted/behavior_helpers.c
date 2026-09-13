/* Generated verbatim upstream function extraction. See extracted.json. */
#include "host/behavior_helpers.h"

#line 1468 "n64decomp/src/game/object_helpers.c"
s32 cur_obj_has_behavior(const BehaviorScript *behavior) {
    if (o->behavior == segmented_to_virtual(behavior)) {
        return TRUE;
    } else {
        return FALSE;
    }
}

#line 764 "n64decomp/src/game/object_helpers.c"
void cur_obj_hide(void) {
    o->header.gfx.node.flags |= GRAPH_RENDER_INVISIBLE;
}

#line 1411 "n64decomp/src/game/object_helpers.c"
void cur_obj_move_xz_using_fvel_and_yaw(void) {
    o->oVelX = o->oForwardVel * sins(o->oMoveAngleYaw);
    o->oVelZ = o->oForwardVel * coss(o->oMoveAngleYaw);

    o->oPosX += o->oVelX;
    o->oPosZ += o->oVelZ;
}

#line 1419 "n64decomp/src/game/object_helpers.c"
void cur_obj_move_y_with_terminal_vel(void) {
    if (o->oVelY < -70.0f) {
        o->oVelY = -70.0f;
    }

    o->oPosY += o->oVelY;
}

#line 711 "n64decomp/src/game/object_helpers.c"
void cur_obj_scale(f32 scale) {
    o->header.gfx.scale[0] = scale;
    o->header.gfx.scale[1] = scale;
    o->header.gfx.scale[2] = scale;
}

#line 384 "n64decomp/src/game/object_helpers.c"
s16 obj_angle_to_object(struct Object *obj1, struct Object *obj2) {
    f32 z1, x1, z2, x2;
    s16 angle;

    z1 = obj1->oPosZ; z2 = obj2->oPosZ; // ordering of instructions..
    x1 = obj1->oPosX; x2 = obj2->oPosX;

    angle = atan2s(z2 - z1, x2 - x1);
    return angle;
}

#line 1936 "n64decomp/src/game/object_helpers.c"
void obj_build_transform_relative_to_parent(struct Object *obj) {
    struct Object *parent = obj->parentObj;

    obj_build_transform_from_pos_and_angle(obj, O_PARENT_RELATIVE_POS_INDEX, O_FACE_ANGLE_INDEX);
    obj_apply_scale_to_transform(obj);
    mtxf_mul(obj->transform, obj->transform, parent->transform);

    obj->oPosX = obj->transform[3][0];
    obj->oPosY = obj->transform[3][1];
    obj->oPosZ = obj->transform[3][2];

    obj->header.gfx.throwMatrix = &obj->transform;

    //! Sets scale of gCurrentObject instead of obj. Not exploitable since this
    //  function is only called with obj = gCurrentObject
    cur_obj_scale(1.0f);
}

#line 613 "n64decomp/src/game/object_helpers.c"
void obj_copy_pos_and_angle(struct Object *dst, struct Object *src) {
    obj_copy_pos(dst, src);
    obj_copy_angle(dst, src);
}

#line 800 "n64decomp/src/game/object_helpers.c"
void obj_set_face_angle_to_move_angle(struct Object *obj) {
    obj->oFaceAnglePitch = obj->oMoveAnglePitch;
    obj->oFaceAngleYaw = obj->oMoveAngleYaw;
    obj->oFaceAngleRoll = obj->oMoveAngleRoll;
}

#line 1923 "n64decomp/src/game/object_helpers.c"
void obj_set_throw_matrix_from_transform(struct Object *obj) {
    if (obj->oFlags & OBJ_FLAG_0020) {
        obj_build_transform_from_pos_and_angle(obj, O_POS_INDEX, O_FACE_ANGLE_INDEX);
        obj_apply_scale_to_transform(obj);
    }

    obj->header.gfx.throwMatrix = &obj->transform;

    //! Sets scale of gCurrentObject instead of obj. Not exploitable since this
    //  function is only called with obj = gCurrentObject
    cur_obj_scale(1.0f);
}

#line 525 "n64decomp/src/game/object_helpers.c"
struct Object *spawn_object_at_origin(struct Object *parent, UNUSED s32 unusedArg, u32 model,
                                      const BehaviorScript *behavior) {
    struct Object *obj;
    const BehaviorScript *behaviorAddr;

    behaviorAddr = segmented_to_virtual(behavior);
    obj = create_object(behaviorAddr);

    obj->parentObj = parent;
    obj->header.gfx.areaIndex = parent->header.gfx.areaIndex;
    obj->header.gfx.activeAreaIndex = parent->header.gfx.areaIndex;

    geo_obj_init((struct GraphNodeObject *) &obj->header.gfx, gLoadedGraphNodes[model], gVec3fZero,
                 gVec3sZero);

    return obj;
}

#line 2413 "n64decomp/src/game/object_helpers.c"
void cur_obj_enable_rendering_if_mario_in_room(void) {
    register s32 marioInRoom;

    if (o->oRoom != -1 && gMarioCurrentRoom != 0) {
        if (gMarioCurrentRoom == o->oRoom) {
            marioInRoom = TRUE;
        } else if (gDoorAdjacentRooms[gMarioCurrentRoom][0] == o->oRoom) {
            marioInRoom = TRUE;
        } else if (gDoorAdjacentRooms[gMarioCurrentRoom][1] == o->oRoom) {
            marioInRoom = TRUE;
        } else {
            marioInRoom = FALSE;
        }

        if (marioInRoom) {
            cur_obj_enable_rendering();
            o->activeFlags &= ~ACTIVE_FLAG_IN_DIFFERENT_ROOM;
            gNumRoomedObjectsInMarioRoom++;
        } else {
            cur_obj_disable_rendering();
            o->activeFlags |= ACTIVE_FLAG_IN_DIFFERENT_ROOM;
            gNumRoomedObjectsNotInMarioRoom++;
        }
    }
}

#line 675 "n64decomp/src/game/object_helpers.c"
void obj_apply_scale_to_transform(struct Object *obj) {
    f32 scaleX = obj->header.gfx.scale[0];
    f32 scaleY = obj->header.gfx.scale[1];
    f32 scaleZ = obj->header.gfx.scale[2];

    obj->transform[0][0] *= scaleX;
    obj->transform[0][1] *= scaleX;
    obj->transform[0][2] *= scaleX;

    obj->transform[1][0] *= scaleY;
    obj->transform[1][1] *= scaleY;
    obj->transform[1][2] *= scaleY;

    obj->transform[2][0] *= scaleZ;
    obj->transform[2][1] *= scaleZ;
    obj->transform[2][2] *= scaleZ;
}

#line 618 "n64decomp/src/game/object_helpers.c"
void obj_copy_pos(struct Object *dst, struct Object *src) {
    dst->oPosX = src->oPosX;
    dst->oPosY = src->oPosY;
    dst->oPosZ = src->oPosZ;
}

#line 624 "n64decomp/src/game/object_helpers.c"
void obj_copy_angle(struct Object *dst, struct Object *src) {
    dst->oMoveAnglePitch = src->oMoveAnglePitch;
    dst->oMoveAngleYaw = src->oMoveAngleYaw;
    dst->oMoveAngleRoll = src->oMoveAngleRoll;

    dst->oFaceAnglePitch = src->oFaceAnglePitch;
    dst->oFaceAngleYaw = src->oFaceAngleYaw;
    dst->oFaceAngleRoll = src->oFaceAngleRoll;
}

#line 543 "n64decomp/src/game/object_helpers.c"
struct Object *spawn_object(struct Object *parent, s32 model, const BehaviorScript *behavior) {
    struct Object *obj = spawn_object_at_origin(parent, 0, model, behavior);

    obj_copy_pos_and_angle(obj, parent);

    return obj;
}

#line 2041 "n64decomp/src/game/object_helpers.c"
f32 random_f32_around_zero(f32 diameter) {
    return random_float() * diameter - diameter / 2;
}

#line 2056 "n64decomp/src/game/object_helpers.c"
void obj_translate_xz_random(struct Object *obj, f32 rangeLength) {
    obj->oPosX += random_float() * rangeLength - rangeLength * 0.5f;
    obj->oPosZ += random_float() * rangeLength - rangeLength * 0.5f;
}

#line 2050 "n64decomp/src/game/object_helpers.c"
void obj_translate_xyz_random(struct Object *obj, f32 rangeLength) {
    obj->oPosX += random_float() * rangeLength - rangeLength * 0.5f;
    obj->oPosY += random_float() * rangeLength - rangeLength * 0.5f;
    obj->oPosZ += random_float() * rangeLength - rangeLength * 0.5f;
}

#line 705 "n64decomp/src/game/object_helpers.c"
void obj_scale(struct Object *obj, f32 scale) {
    obj->header.gfx.scale[0] = scale;
    obj->header.gfx.scale[1] = scale;
    obj->header.gfx.scale[2] = scale;
}

#line 2373 "n64decomp/src/game/object_helpers.c"
s32 is_item_in_array(s8 item, s8 *array) {
    while (*array != -1) {
        if (*array == item) {
            return TRUE;
        }

        array++;
    }

    return FALSE;
}

#line 32 "n64decomp/src/game/object_helpers.c"
static s8 sLevelsWithRooms[] = { LEVEL_BBH, LEVEL_CASTLE, LEVEL_HMC, -1 };

#line 2388 "n64decomp/src/game/object_helpers.c"
void bhv_init_room(void) {
    struct Surface *floor;
    f32 floorHeight;

    if (is_item_in_array(gCurrLevelNum, sLevelsWithRooms)) {
        floorHeight = find_floor(o->oPosX, o->oPosY, o->oPosZ, &floor);

        if (floor != NULL) {
            if (floor->room != 0) {
                o->oRoom = floor->room;
            } else {
                // Floor probably belongs to a platform object. Try looking
                // underneath it
                find_floor(o->oPosX, floorHeight - 100.0f, o->oPosZ, &floor);
                if (floor != NULL) {
                    //! Technically possible that the room could still be 0 here
                    o->oRoom = floor->room;
                }
            }
        }
    } else {
        o->oRoom = -1;
    }
}

#line 747 "n64decomp/src/game/object_helpers.c"
void cur_obj_enable_rendering(void) {
    o->header.gfx.node.flags |= GRAPH_RENDER_ACTIVE;
}

#line 756 "n64decomp/src/game/object_helpers.c"
void cur_obj_disable_rendering(void) {
    o->header.gfx.node.flags &= ~GRAPH_RENDER_ACTIVE;
}

#line 486 "n64decomp/src/game/object_helpers.c"
struct Object *spawn_water_droplet(struct Object *parent, struct WaterDropletParams *params) {
    f32 randomScale;
    struct Object *newObj = spawn_object(parent, params->model, params->behavior);

    if (params->flags & WATER_DROPLET_FLAG_RAND_ANGLE) {
        newObj->oMoveAngleYaw = random_u16();
    }

    if (params->flags & WATER_DROPLET_FLAG_RAND_ANGLE_INCR_PLUS_8000) {
        newObj->oMoveAngleYaw = (s16)(newObj->oMoveAngleYaw + 0x8000)
                                + (s16) random_f32_around_zero(params->moveAngleRange);
    }

    if (params->flags & WATER_DROPLET_FLAG_RAND_ANGLE_INCR) {
        newObj->oMoveAngleYaw =
            (s16) newObj->oMoveAngleYaw + (s16) random_f32_around_zero(params->moveAngleRange);
    }

    if (params->flags & WATER_DROPLET_FLAG_SET_Y_TO_WATER_LEVEL) {
        newObj->oPosY = find_water_level(newObj->oPosX, newObj->oPosZ);
    }

    if (params->flags & WATER_DROPLET_FLAG_RAND_OFFSET_XZ) {
        obj_translate_xz_random(newObj, params->moveRange);
    }

    if (params->flags & WATER_DROPLET_FLAG_RAND_OFFSET_XYZ) {
        obj_translate_xyz_random(newObj, params->moveRange);
    }

    newObj->oForwardVel = random_float() * params->randForwardVelScale + params->randForwardVelOffset;
    newObj->oVelY = random_float() * params->randYVelScale + params->randYVelOffset;

    randomScale = random_float() * params->randSizeScale + params->randSizeOffset;
    obj_scale(newObj, randomScale);

    return newObj;
}
