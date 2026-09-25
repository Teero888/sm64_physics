// moat_drainer.inc.c

void bhv_invisible_objects_under_bridge_init(void) {
    if (save_file_get_flags() & SAVE_FLAG_MOAT_DRAINED) {
        WORLD(gEnvironmentRegions)[6] = -800;
        WORLD(gEnvironmentRegions)[12] = -800;
    }
}
