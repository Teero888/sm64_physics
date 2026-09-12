#include "engine/graph_node.h"

/* Native object bookkeeping; these nodes have no GPU resources or display
 * lists. They remain because the decompiled object allocator maintains their
 * links and transforms. The active world will own this state. */
struct GraphNode gObjParentGraphNode;
Vec3f gVec3fZero = {0.0f, 0.0f, 0.0f};
Vec3s gVec3sZero = {0, 0, 0};
Vec3f gVec3fOne = {1.0f, 1.0f, 1.0f};
