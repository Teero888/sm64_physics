/* Generated verbatim upstream function extraction. See extracted.json. */
#include "host/behavior_state.h"

#line 84 "n64decomp/src/engine/behavior_script.c"
void obj_update_gfx_pos_and_angle(struct Object *obj) {
    obj->header.gfx.pos[0] = obj->oPosX;
    obj->header.gfx.pos[1] = obj->oPosY + obj->oGraphYOffset;
    obj->header.gfx.pos[2] = obj->oPosZ;

    obj->header.gfx.angle[0] = obj->oFaceAnglePitch & 0xFFFF;
    obj->header.gfx.angle[1] = obj->oFaceAngleYaw & 0xFFFF;
    obj->header.gfx.angle[2] = obj->oFaceAngleRoll & 0xFFFF;
}

#line 95 "n64decomp/src/engine/behavior_script.c"
static void cur_obj_bhv_stack_push(uintptr_t bhvAddr) {
    gCurrentObject->bhvStack[gCurrentObject->bhvStackIndex] = bhvAddr;
    gCurrentObject->bhvStackIndex++;
}

#line 101 "n64decomp/src/engine/behavior_script.c"
static uintptr_t cur_obj_bhv_stack_pop(void) {
    uintptr_t bhvAddr;

    gCurrentObject->bhvStackIndex--;
    bhvAddr = gCurrentObject->bhvStack[gCurrentObject->bhvStackIndex];

    return bhvAddr;
}

#line 118 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_hide(void) {
    cur_obj_hide();

    gCurBhvCommand++;
    return BHV_PROC_CONTINUE;
}

#line 127 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_disable_rendering(void) {
    gCurrentObject->header.gfx.node.flags &= ~GRAPH_RENDER_ACTIVE;

    gCurBhvCommand++;
    return BHV_PROC_CONTINUE;
}

#line 136 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_billboard(void) {
    gCurrentObject->header.gfx.node.flags |= GRAPH_RENDER_BILLBOARD;

    gCurBhvCommand++;
    return BHV_PROC_CONTINUE;
}

#line 145 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_set_model(void) {
    s32 modelID = BHV_CMD_GET_2ND_S16(0);

    gCurrentObject->header.gfx.sharedChild = gLoadedGraphNodes[modelID];

    gCurBhvCommand++;
    return BHV_PROC_CONTINUE;
}

#line 156 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_spawn_child(void) {
    u32 model = BHV_CMD_GET_U32(1);
    const BehaviorScript *behavior = BHV_CMD_GET_VPTR(2);

    struct Object *child = spawn_object_at_origin(gCurrentObject, 0, model, behavior);
    obj_copy_pos_and_angle(child, gCurrentObject);

    gCurBhvCommand += 3;
    return BHV_PROC_CONTINUE;
}

#line 169 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_spawn_obj(void) {
    u32 model = BHV_CMD_GET_U32(1);
    const BehaviorScript *behavior = BHV_CMD_GET_VPTR(2);

    struct Object *object = spawn_object_at_origin(gCurrentObject, 0, model, behavior);
    obj_copy_pos_and_angle(object, gCurrentObject);
    // TODO: Does this cmd need renaming? This line is the only difference between this and the above func.
    gCurrentObject->prevObj = object;

    gCurBhvCommand += 3;
    return BHV_PROC_CONTINUE;
}

#line 184 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_spawn_child_with_param(void) {
    u32 bhvParam = BHV_CMD_GET_2ND_S16(0);
    u32 modelID = BHV_CMD_GET_U32(1);
    const BehaviorScript *behavior = BHV_CMD_GET_VPTR(2);

    struct Object *child = spawn_object_at_origin(gCurrentObject, 0, modelID, behavior);
    obj_copy_pos_and_angle(child, gCurrentObject);
    child->oBhvParams2ndByte = bhvParam;

    gCurBhvCommand += 3;
    return BHV_PROC_CONTINUE;
}

#line 199 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_deactivate(void) {
    gCurrentObject->activeFlags = ACTIVE_FLAG_DEACTIVATED;
    return BHV_PROC_BREAK;
}

#line 206 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_break(void) {
    return BHV_PROC_BREAK;
}

#line 212 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_break_unused(void) {
    return BHV_PROC_BREAK;
}

#line 218 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_call(void) {
    const BehaviorScript *jumpAddress;
    gCurBhvCommand++;

    cur_obj_bhv_stack_push(BHV_CMD_GET_ADDR_OF_CMD(1)); // Store address of the next bhv command in the stack.
    jumpAddress = segmented_to_virtual(BHV_CMD_GET_VPTR(0));
    gCurBhvCommand = jumpAddress; // Jump to the new address.

    return BHV_PROC_CONTINUE;
}

#line 231 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_return(void) {
    gCurBhvCommand = (const BehaviorScript *) cur_obj_bhv_stack_pop(); // Retrieve command address and jump to it.
    return BHV_PROC_CONTINUE;
}

#line 238 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_delay(void) {
    s16 num = BHV_CMD_GET_2ND_S16(0);

    if (gCurrentObject->bhvDelayTimer < num - 1) {
        gCurrentObject->bhvDelayTimer++; // Increment timer
    } else {
        gCurrentObject->bhvDelayTimer = 0;
        gCurBhvCommand++; // Delay ended, move to next bhv command (note: following commands will not execute until next frame)
    }

    return BHV_PROC_BREAK;
}

#line 253 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_delay_var(void) {
    u8 field = BHV_CMD_GET_2ND_U8(0);
    s32 num = cur_obj_get_int(field);

    if (gCurrentObject->bhvDelayTimer < num - 1) {
        gCurrentObject->bhvDelayTimer++; // Increment timer
    } else {
        gCurrentObject->bhvDelayTimer = 0;
        gCurBhvCommand++; // Delay ended, move to next bhv command
    }

    return BHV_PROC_BREAK;
}

#line 269 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_goto(void) {
    gCurBhvCommand++; // Useless
    gCurBhvCommand = segmented_to_virtual(BHV_CMD_GET_VPTR(0)); // Jump directly to address
    return BHV_PROC_CONTINUE;
}

#line 278 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_begin_repeat_unused(void) {
    s32 count = BHV_CMD_GET_2ND_U8(0);

    cur_obj_bhv_stack_push(BHV_CMD_GET_ADDR_OF_CMD(1)); // Store address of the first command of the loop in the stack
    cur_obj_bhv_stack_push(count); // Store repeat count in the stack too

    gCurBhvCommand++;
    return BHV_PROC_CONTINUE;
}

#line 290 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_begin_repeat(void) {
    s32 count = BHV_CMD_GET_2ND_S16(0);

    cur_obj_bhv_stack_push(BHV_CMD_GET_ADDR_OF_CMD(1)); // Store address of the first command of the loop in the stack
    cur_obj_bhv_stack_push(count); // Store repeat count in the stack too

    gCurBhvCommand++;
    return BHV_PROC_CONTINUE;
}

#line 302 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_end_repeat(void) {
    u32 count = cur_obj_bhv_stack_pop(); // Retrieve loop count from the stack.
    count--;

    if (count != 0) {
        gCurBhvCommand = (const BehaviorScript *) cur_obj_bhv_stack_pop(); // Jump back to the first command in the loop
        // Save address and count to the stack again
        cur_obj_bhv_stack_push(BHV_CMD_GET_ADDR_OF_CMD(0));
        cur_obj_bhv_stack_push(count);
    } else { // Finished iterating over the loop
        cur_obj_bhv_stack_pop(); // Necessary to remove address from the stack
        gCurBhvCommand++;
    }

    // Don't execute following commands until next frame
    return BHV_PROC_BREAK;
}

#line 322 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_end_repeat_continue(void) {
    u32 count = cur_obj_bhv_stack_pop();
    count--;

    if (count != 0) {
        gCurBhvCommand = (const BehaviorScript *) cur_obj_bhv_stack_pop(); // Jump back to the first command in the loop
        // Save address and count to the stack again
        cur_obj_bhv_stack_push(BHV_CMD_GET_ADDR_OF_CMD(0));
        cur_obj_bhv_stack_push(count);
    } else { // Finished iterating over the loop
        cur_obj_bhv_stack_pop(); // Necessary to remove address from the stack
        gCurBhvCommand++;
    }

    // Start executing following commands immediately
    return BHV_PROC_CONTINUE;
}

#line 342 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_begin_loop(void) {
    cur_obj_bhv_stack_push(BHV_CMD_GET_ADDR_OF_CMD(1)); // Store address of the first command of the loop in the stack

    gCurBhvCommand++;
    return BHV_PROC_CONTINUE;
}

#line 351 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_end_loop(void) {
    gCurBhvCommand = (const BehaviorScript *) cur_obj_bhv_stack_pop(); // Jump back to the first command in the loop
    cur_obj_bhv_stack_push(BHV_CMD_GET_ADDR_OF_CMD(0)); // Save address to the stack again

    return BHV_PROC_BREAK;
}

#line 361 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_call_native(void) {
    NativeBhvFunc behaviorFunc = BHV_CMD_GET_VPTR(1);

    behaviorFunc();

    gCurBhvCommand += 2;
    return BHV_PROC_CONTINUE;
}

#line 372 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_set_float(void) {
    u8 field = BHV_CMD_GET_2ND_U8(0);
    f32 value = BHV_CMD_GET_2ND_S16(0);

    cur_obj_set_float(field, value);

    gCurBhvCommand++;
    return BHV_PROC_CONTINUE;
}

#line 384 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_set_int(void) {
    u8 field = BHV_CMD_GET_2ND_U8(0);
    s16 value = BHV_CMD_GET_2ND_S16(0);

    cur_obj_set_int(field, value);

    gCurBhvCommand++;
    return BHV_PROC_CONTINUE;
}

#line 395 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_set_int_unused(void) {
    u8 field = BHV_CMD_GET_2ND_U8(0);
    s32 value = BHV_CMD_GET_2ND_S16(1); // Taken from 2nd word instead of 1st

    cur_obj_set_int(field, value);

    gCurBhvCommand += 2; // Twice as long
    return BHV_PROC_CONTINUE;
}

#line 407 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_set_random_float(void) {
    u8 field = BHV_CMD_GET_2ND_U8(0);
    f32 min = BHV_CMD_GET_2ND_S16(0);
    f32 range = BHV_CMD_GET_1ST_S16(1);

    cur_obj_set_float(field, (range * random_float()) + min);

    gCurBhvCommand += 2;
    return BHV_PROC_CONTINUE;
}

#line 420 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_set_random_int(void) {
    u8 field = BHV_CMD_GET_2ND_U8(0);
    s32 min = BHV_CMD_GET_2ND_S16(0);
    s32 range = BHV_CMD_GET_1ST_S16(1);

    cur_obj_set_int(field, (s32)(range * random_float()) + min);

    gCurBhvCommand += 2;
    return BHV_PROC_CONTINUE;
}

#line 433 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_set_int_rand_rshift(void) {
    u8 field = BHV_CMD_GET_2ND_U8(0);
    s32 min = BHV_CMD_GET_2ND_S16(0);
    s32 rshift = BHV_CMD_GET_1ST_S16(1);

    cur_obj_set_int(field, (random_u16() >> rshift) + min);

    gCurBhvCommand += 2;
    return BHV_PROC_CONTINUE;
}

#line 446 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_add_random_float(void) {
    u8 field = BHV_CMD_GET_2ND_U8(0);
    f32 min = BHV_CMD_GET_2ND_S16(0);
    f32 range = BHV_CMD_GET_1ST_S16(1);

    cur_obj_set_float(field, cur_obj_get_float(field) + min + (range * random_float()));

    gCurBhvCommand += 2;
    return BHV_PROC_CONTINUE;
}

#line 459 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_add_int_rand_rshift(void) {
    u8 field = BHV_CMD_GET_2ND_U8(0);
    s32 min = BHV_CMD_GET_2ND_S16(0);
    s32 rshift = BHV_CMD_GET_1ST_S16(1);
    s32 rnd = random_u16();

    cur_obj_set_int(field, (cur_obj_get_int(field) + min) + (rnd >> rshift));

    gCurBhvCommand += 2;
    return BHV_PROC_CONTINUE;
}

#line 473 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_add_float(void) {
    u8 field = BHV_CMD_GET_2ND_U8(0);
    f32 value = BHV_CMD_GET_2ND_S16(0);

    cur_obj_add_float(field, value);

    gCurBhvCommand++;
    return BHV_PROC_CONTINUE;
}

#line 485 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_add_int(void) {
    u8 field = BHV_CMD_GET_2ND_U8(0);
    s16 value = BHV_CMD_GET_2ND_S16(0);

    cur_obj_add_int(field, value);

    gCurBhvCommand++;
    return BHV_PROC_CONTINUE;
}

#line 498 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_or_int(void) {
    u8 objectOffset = BHV_CMD_GET_2ND_U8(0);
    s32 value = BHV_CMD_GET_2ND_S16(0);

    value &= 0xFFFF;
    cur_obj_or_int(objectOffset, value);

    gCurBhvCommand++;
    return BHV_PROC_CONTINUE;
}

#line 511 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_bit_clear(void) {
    u8 field = BHV_CMD_GET_2ND_U8(0);
    s32 value = BHV_CMD_GET_2ND_S16(0);

    value = (value & 0xFFFF) ^ 0xFFFF;
    cur_obj_and_int(field, value);

    gCurBhvCommand++;
    return BHV_PROC_CONTINUE;
}

#line 524 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_load_animations(void) {
    u8 field = BHV_CMD_GET_2ND_U8(0);

    cur_obj_set_vptr(field, BHV_CMD_GET_VPTR(1));

    gCurBhvCommand += 2;
    return BHV_PROC_CONTINUE;
}

#line 535 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_animate(void) {
    s32 animIndex = BHV_CMD_GET_2ND_U8(0);
    struct Animation **animations = gCurrentObject->oAnimations;

    geo_obj_init_animation(&gCurrentObject->header.gfx, &animations[animIndex]);

    gCurBhvCommand++;
    return BHV_PROC_CONTINUE;
}

#line 547 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_drop_to_floor(void) {
    f32 x = gCurrentObject->oPosX;
    f32 y = gCurrentObject->oPosY;
    f32 z = gCurrentObject->oPosZ;

    f32 floor = find_floor_height(x, y + 200.0f, z);
    gCurrentObject->oPosY = floor;
    gCurrentObject->oMoveFlags |= OBJ_MOVE_ON_GROUND;

    gCurBhvCommand++;
    return BHV_PROC_CONTINUE;
}

#line 562 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_nop_1(void) {
    UNUSED u8 field = BHV_CMD_GET_2ND_U8(0);

    gCurBhvCommand++;
    return BHV_PROC_CONTINUE;
}

#line 571 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_nop_3(void) {
    UNUSED u8 field = BHV_CMD_GET_2ND_U8(0);

    gCurBhvCommand++;
    return BHV_PROC_CONTINUE;
}

#line 580 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_nop_2(void) {
    UNUSED u8 field = BHV_CMD_GET_2ND_U8(0);

    gCurBhvCommand++;
    return BHV_PROC_CONTINUE;
}

#line 589 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_sum_float(void) {
    u32 fieldDst = BHV_CMD_GET_2ND_U8(0);
    u32 fieldSrc1 = BHV_CMD_GET_3RD_U8(0);
    u32 fieldSrc2 = BHV_CMD_GET_4TH_U8(0);

    cur_obj_set_float(fieldDst, cur_obj_get_float(fieldSrc1) + cur_obj_get_float(fieldSrc2));

    gCurBhvCommand++;
    return BHV_PROC_CONTINUE;
}

#line 602 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_sum_int(void) {
    u32 fieldDst = BHV_CMD_GET_2ND_U8(0);
    u32 fieldSrc1 = BHV_CMD_GET_3RD_U8(0);
    u32 fieldSrc2 = BHV_CMD_GET_4TH_U8(0);

    cur_obj_set_int(fieldDst, cur_obj_get_int(fieldSrc1) + cur_obj_get_int(fieldSrc2));

    gCurBhvCommand++;
    return BHV_PROC_CONTINUE;
}

#line 615 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_set_hitbox(void) {
    s16 radius = BHV_CMD_GET_1ST_S16(1);
    s16 height = BHV_CMD_GET_2ND_S16(1);

    gCurrentObject->hitboxRadius = radius;
    gCurrentObject->hitboxHeight = height;

    gCurBhvCommand += 2;
    return BHV_PROC_CONTINUE;
}

#line 628 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_set_hurtbox(void) {
    s16 radius = BHV_CMD_GET_1ST_S16(1);
    s16 height = BHV_CMD_GET_2ND_S16(1);

    gCurrentObject->hurtboxRadius = radius;
    gCurrentObject->hurtboxHeight = height;

    gCurBhvCommand += 2;
    return BHV_PROC_CONTINUE;
}

#line 641 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_set_hitbox_with_offset(void) {
    s16 radius = BHV_CMD_GET_1ST_S16(1);
    s16 height = BHV_CMD_GET_2ND_S16(1);
    s16 downOffset = BHV_CMD_GET_1ST_S16(2);

    gCurrentObject->hitboxRadius = radius;
    gCurrentObject->hitboxHeight = height;
    gCurrentObject->hitboxDownOffset = downOffset;

    gCurBhvCommand += 3;
    return BHV_PROC_CONTINUE;
}

#line 656 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_nop_4(void) {
    UNUSED s16 field = BHV_CMD_GET_2ND_U8(0);
    UNUSED s16 value = BHV_CMD_GET_2ND_S16(0);

    gCurBhvCommand++;
    return BHV_PROC_CONTINUE;
}

#line 667 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_begin(void) {
    // These objects were likely very early objects, which is why this code is here
    // instead of in the respective behavior scripts.

    // Initiate the room if the object is a haunted chair or the mad piano.
    if (cur_obj_has_behavior(bhvHauntedChair)) {
        bhv_init_room();
    }
    if (cur_obj_has_behavior(bhvMadPiano)) {
        bhv_init_room();
    }
    // Set collision distance if the object is a message panel.
    if (cur_obj_has_behavior(bhvMessagePanel)) {
        gCurrentObject->oCollisionDistance = 150.0f;
    }
    gCurBhvCommand++;
    return BHV_PROC_CONTINUE;
}

#line 732 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_load_collision_data(void) {
    u32 *collisionData = segmented_to_virtual(BHV_CMD_GET_VPTR(1));

    gCurrentObject->collisionData = collisionData;

    gCurBhvCommand += 2;
    return BHV_PROC_CONTINUE;
}

#line 743 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_set_home(void) {
    gCurrentObject->oHomeX = gCurrentObject->oPosX;
    gCurrentObject->oHomeY = gCurrentObject->oPosY;
    gCurrentObject->oHomeZ = gCurrentObject->oPosZ;

    gCurBhvCommand++;
    return BHV_PROC_CONTINUE;
}

#line 754 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_set_interact_type(void) {
    gCurrentObject->oInteractType = BHV_CMD_GET_U32(1);

    gCurBhvCommand += 2;
    return BHV_PROC_CONTINUE;
}

#line 763 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_set_interact_subtype(void) {
    gCurrentObject->oInteractionSubtype = BHV_CMD_GET_U32(1);

    gCurBhvCommand += 2;
    return BHV_PROC_CONTINUE;
}

#line 772 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_scale(void) {
    UNUSED u8 unusedField = BHV_CMD_GET_2ND_U8(0);
    s16 percent = BHV_CMD_GET_2ND_S16(0);

    cur_obj_scale(percent / 100.0f);

    gCurBhvCommand++;
    return BHV_PROC_CONTINUE;
}

#line 785 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_set_obj_physics(void) {
    UNUSED f32 unused1, unused2;

    gCurrentObject->oWallHitboxRadius = BHV_CMD_GET_1ST_S16(1);
    gCurrentObject->oGravity = BHV_CMD_GET_2ND_S16(1) / 100.0f;
    gCurrentObject->oBounciness = BHV_CMD_GET_1ST_S16(2) / 100.0f;
    gCurrentObject->oDragStrength = BHV_CMD_GET_2ND_S16(2) / 100.0f;
    gCurrentObject->oFriction = BHV_CMD_GET_1ST_S16(3) / 100.0f;
    gCurrentObject->oBuoyancy = BHV_CMD_GET_2ND_S16(3) / 100.0f;

    unused1 = BHV_CMD_GET_1ST_S16(4) / 100.0f;
    unused2 = BHV_CMD_GET_2ND_S16(4) / 100.0f;

    gCurBhvCommand += 5;
    return BHV_PROC_CONTINUE;
}

#line 805 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_parent_bit_clear(void) {
    u8 field = BHV_CMD_GET_2ND_U8(0);
    s32 value = BHV_CMD_GET_U32(1);

    value = value ^ 0xFFFFFFFF;
    obj_and_int(gCurrentObject->parentObj, field, value);

    gCurBhvCommand += 2;
    return BHV_PROC_CONTINUE;
}

#line 818 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_spawn_water_droplet(void) {
    struct WaterDropletParams *dropletParams = BHV_CMD_GET_VPTR(1);

    spawn_water_droplet(gCurrentObject, dropletParams);

    gCurBhvCommand += 2;
    return BHV_PROC_CONTINUE;
}

#line 829 "n64decomp/src/engine/behavior_script.c"
static s32 bhv_cmd_animate_texture(void) {
    u8 field = BHV_CMD_GET_2ND_U8(0);
    s16 rate = BHV_CMD_GET_2ND_S16(0);

    // Increase the field (oAnimState) by 1 every <rate> frames.
    if ((gGlobalTimer % rate) == 0) {
        cur_obj_add_int(field, 1);
    }

    gCurBhvCommand++;
    return BHV_PROC_CONTINUE;
}

#line 842 "n64decomp/src/engine/behavior_script.c"
void stub_behavior_script_2(void) {
}

#line 846 "n64decomp/src/engine/behavior_script.c"
static BhvCommandProc BehaviorCmdTable[] = {
    bhv_cmd_begin,
    bhv_cmd_delay,
    bhv_cmd_call,
    bhv_cmd_return,
    bhv_cmd_goto,
    bhv_cmd_begin_repeat,
    bhv_cmd_end_repeat,
    bhv_cmd_end_repeat_continue,
    bhv_cmd_begin_loop,
    bhv_cmd_end_loop,
    bhv_cmd_break,
    bhv_cmd_break_unused,
    bhv_cmd_call_native,
    bhv_cmd_add_float,
    bhv_cmd_set_float,
    bhv_cmd_add_int,
    bhv_cmd_set_int,
    bhv_cmd_or_int,
    bhv_cmd_bit_clear,
    bhv_cmd_set_int_rand_rshift,
    bhv_cmd_set_random_float,
    bhv_cmd_set_random_int,
    bhv_cmd_add_random_float,
    bhv_cmd_add_int_rand_rshift,
    bhv_cmd_nop_1,
    bhv_cmd_nop_2,
    bhv_cmd_nop_3,
    bhv_cmd_set_model,
    bhv_cmd_spawn_child,
    bhv_cmd_deactivate,
    bhv_cmd_drop_to_floor,
    bhv_cmd_sum_float,
    bhv_cmd_sum_int,
    bhv_cmd_billboard,
    bhv_cmd_hide,
    bhv_cmd_set_hitbox,
    bhv_cmd_nop_4,
    bhv_cmd_delay_var,
    bhv_cmd_begin_repeat_unused,
    bhv_cmd_load_animations,
    bhv_cmd_animate,
    bhv_cmd_spawn_child_with_param,
    bhv_cmd_load_collision_data,
    bhv_cmd_set_hitbox_with_offset,
    bhv_cmd_spawn_obj,
    bhv_cmd_set_home,
    bhv_cmd_set_hurtbox,
    bhv_cmd_set_interact_type,
    bhv_cmd_set_obj_physics,
    bhv_cmd_set_interact_subtype,
    bhv_cmd_scale,
    bhv_cmd_parent_bit_clear,
    bhv_cmd_animate_texture,
    bhv_cmd_disable_rendering,
    bhv_cmd_set_int_unused,
    bhv_cmd_spawn_water_droplet,
};

#line 906 "n64decomp/src/engine/behavior_script.c"
void cur_obj_update(void) {
    UNUSED u8 filler[4];

    s16 objFlags = gCurrentObject->oFlags;
    f32 distanceFromMario;
    BhvCommandProc bhvCmdProc;
    s32 bhvProcResult;

    // Calculate the distance from the object to Mario.
    if (objFlags & OBJ_FLAG_COMPUTE_DIST_TO_MARIO) {
        gCurrentObject->oDistanceToMario = dist_between_objects(gCurrentObject, gMarioObject);
        distanceFromMario = gCurrentObject->oDistanceToMario;
    } else {
        distanceFromMario = 0.0f;
    }

    // Calculate the angle from the object to Mario.
    if (objFlags & OBJ_FLAG_COMPUTE_ANGLE_TO_MARIO) {
        gCurrentObject->oAngleToMario = obj_angle_to_object(gCurrentObject, gMarioObject);
    }

    // If the object's action has changed, reset the action timer.
    if (gCurrentObject->oAction != gCurrentObject->oPrevAction) {
        (void) (gCurrentObject->oTimer = 0, gCurrentObject->oSubAction = 0,
                gCurrentObject->oPrevAction = gCurrentObject->oAction);
    }

    // Execute the behavior script.
    gCurBhvCommand = gCurrentObject->curBhvCommand;

    do {
        bhvCmdProc = BehaviorCmdTable[*gCurBhvCommand >> 24];
        bhvProcResult = bhvCmdProc();
    } while (bhvProcResult == BHV_PROC_CONTINUE);

    gCurrentObject->curBhvCommand = gCurBhvCommand;

    // Increment the object's timer.
    if (gCurrentObject->oTimer < 0x3FFFFFFF) {
        gCurrentObject->oTimer++;
    }

    // If the object's action has changed, reset the action timer.
    if (gCurrentObject->oAction != gCurrentObject->oPrevAction) {
        (void) (gCurrentObject->oTimer = 0, gCurrentObject->oSubAction = 0,
                gCurrentObject->oPrevAction = gCurrentObject->oAction);
    }

    // Execute various code based on object flags.
    objFlags = (s16) gCurrentObject->oFlags;

    if (objFlags & OBJ_FLAG_SET_FACE_ANGLE_TO_MOVE_ANGLE) {
        obj_set_face_angle_to_move_angle(gCurrentObject);
    }

    if (objFlags & OBJ_FLAG_SET_FACE_YAW_TO_MOVE_YAW) {
        gCurrentObject->oFaceAngleYaw = gCurrentObject->oMoveAngleYaw;
    }

    if (objFlags & OBJ_FLAG_MOVE_XZ_USING_FVEL) {
        cur_obj_move_xz_using_fvel_and_yaw();
    }

    if (objFlags & OBJ_FLAG_MOVE_Y_WITH_TERMINAL_VEL) {
        cur_obj_move_y_with_terminal_vel();
    }

    if (objFlags & OBJ_FLAG_TRANSFORM_RELATIVE_TO_PARENT) {
        obj_build_transform_relative_to_parent(gCurrentObject);
    }

    if (objFlags & OBJ_FLAG_SET_THROW_MATRIX_FROM_TRANSFORM) {
        obj_set_throw_matrix_from_transform(gCurrentObject);
    }

    if (objFlags & OBJ_FLAG_UPDATE_GFX_POS_AND_ANGLE) {
        obj_update_gfx_pos_and_angle(gCurrentObject);
    }

    // Handle visibility of object
    if (gCurrentObject->oRoom != -1) {
        // If the object is in a room, only show it when Mario is in the room.
        cur_obj_enable_rendering_if_mario_in_room();
    } else if ((objFlags & OBJ_FLAG_COMPUTE_DIST_TO_MARIO) && gCurrentObject->collisionData == NULL) {
        if (!(objFlags & OBJ_FLAG_ACTIVE_FROM_AFAR)) {
            // If the object has a render distance, check if it should be shown.
            if (distanceFromMario > gCurrentObject->oDrawingDistance) {
                // Out of render distance, hide the object.
                gCurrentObject->header.gfx.node.flags &= ~GRAPH_RENDER_ACTIVE;
                gCurrentObject->activeFlags |= ACTIVE_FLAG_FAR_AWAY;
            } else if (gCurrentObject->oHeldState == HELD_FREE) {
                // In render distance (and not being held), show the object.
                gCurrentObject->header.gfx.node.flags |= GRAPH_RENDER_ACTIVE;
                gCurrentObject->activeFlags &= ~ACTIVE_FLAG_FAR_AWAY;
            }
        }
    }
}
