/* The game's state as one section (sm64_physics.h, docs/state.md): the
   writable variables of the three section groups, in a fixed order with
   their boundaries, page aligned so the template can be protected. Used for
   the relocatable link that makes the library's object. */
SECTIONS
{
  sm64_state : ALIGN(65536)
  {
    sm64_state_start = .;
    *(sm64st_data) *(sm64st_datarel) *(sm64st_datarel2) *(sm64st_bss)
    sm64_overlay_start = .;
    *(sm64ovl_data) *(sm64ovl_datarel) *(sm64ovl_datarel2)
    sm64_overlay_bss = .;
    *(sm64ovl_bss)
    sm64_overlay_end = .;
    sm64_level_data_start = .;
    *(sm64lvl_data) *(sm64lvl_datarel) *(sm64lvl_datarel2)
    sm64_level_data_bss = .;
    *(sm64lvl_bss)
    sm64_level_data_end = .;
    . = ALIGN(65536);
    sm64_state_end = .;
  }
}
