/* em/terreno (TERRAIN) sul digikit nuovo (13/11): la dichiarazione della machine, che prima era [[machine]] + [[voice]] in module.toml.
   Le 24 manopole restano righe [[entry]] del registro params (slot 25 .. 48, template/otpl), le pagine le disegna em/pagine:
   qui niente DK_PARAM ne' DK_PAGE. Il numero di tipo (8, salvato nei kit) e' in core/adapters/machines/types.toml. */
#include <dk/machine/machine.h>

DK_MACHINE(terreno, .name = "Terrain", .short_name = "TER", .render = "_ter_render",
           .note_on = "_ter_note_on", .desc = "_ter_desc");
