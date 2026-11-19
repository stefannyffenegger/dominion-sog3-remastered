/* Game logic: angles, sine and cosine, colour map (trig.c) */
#ifndef DOMINION_ISO_TRIG_H
#define DOMINION_ISO_TRIG_H

#include <stdint.h>

int32_t __stdcall iso_angle(uint8_t *iso, int32_t dx, int32_t dy);
int32_t __stdcall iso_cos(uint8_t *iso, int32_t a);
int32_t __stdcall iso_sin(uint8_t *iso, int32_t a);
void __cdecl flags_clear_bit3(uint32_t *f);
void __cdecl flags_set_field16(uint32_t *f, int16_t v);
void __cdecl flags_clear_field16(uint32_t *f);
int32_t __cdecl colormap_load(void);

#endif
