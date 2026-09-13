/* Generated verbatim upstream function extraction. See extracted.json. */
#include "host/objects_state.h"

#line 868 "n64decomp/src/game/object_helpers.c"
struct Object *find_unimportant_object(void) {
    struct ObjectNode *listHead = &gObjectLists[OBJ_LIST_UNIMPORTANT];
    struct ObjectNode *obj = listHead->next;

    if (listHead == obj) {
        obj = NULL;
    }

    return (struct Object *) obj;
}

#line 879 "n64decomp/src/game/object_helpers.c"
s32 count_unimportant_objects(void) {
    struct ObjectNode *listHead = &gObjectLists[OBJ_LIST_UNIMPORTANT];
    struct ObjectNode *obj = listHead->next;
    s32 count = 0;

    while (listHead != obj) {
        count++;
        obj = obj->next;
    }

    return count;
}
