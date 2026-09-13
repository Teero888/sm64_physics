#ifndef SM64_PHYSICS_BEHAVIOR_H
#define SM64_PHYSICS_BEHAVIOR_H
#include "objects.h"
/* Borrowed native model nodes and special behavior identities. A NULL identity
 * means that behavior is absent from the host's asset set. Model IDs accessed
 * by trusted bytecode must index a valid entry in the supplied table. */
void sm64_objects_bind_behavior_assets(struct sm64_objects *objects, struct GraphNode **models,
    const BehaviorScript *chair, const BehaviorScript *piano, const BehaviorScript *panel);
/* Borrowed resolved scripts. The three carry scripts must be non-NULL and
 * distinct; non-holdable objects execute them after a grab/drop/throw. */
void sm64_objects_bind_carry_assets(struct sm64_objects *objects,
    const BehaviorScript *held, const BehaviorScript *dropped,
    const BehaviorScript *thrown, const BehaviorScript *underwater_shell);
/* One original cur_obj_update, not the full world's frame scheduler. Requires
 * active objects, terrain, audio and valid resolved bytecode/native callbacks.
 * The object and every object referenced by its script must belong to this
 * context. The original interpreter's stack/opcode/loop preconditions apply. */
void sm64_objects_update_behavior(struct Object *object, u32 frame);
#endif
