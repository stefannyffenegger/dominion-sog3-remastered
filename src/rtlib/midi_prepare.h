/* Preparing MIDI data for the engine's sequencer (midi_prepare.c) */
#ifndef DOMINION_RTLIB_MIDI_PREPARE_H
#define DOMINION_RTLIB_MIDI_PREPARE_H

#include <stdint.h>

#ifndef __stdcall
#define __stdcall __attribute__((stdcall))
#endif

int __cdecl midi_read_header(void);
uint32_t __cdecl midi_read_varlen(void);
int32_t __stdcall midi_find_slot(uint32_t t);
int __stdcall midi_add_event(uint32_t delta, uint32_t msg);
int __stdcall midi_add_tempo(uint32_t delta, uint32_t tempo);
int __stdcall midi_channel_event(uint32_t delta, uint8_t status);
int __stdcall midi_meta(uint32_t delta);
int __stdcall midi_sysex(uint32_t delta);
int __stdcall midi_read_events(uint32_t len);
int __stdcall midi_read_track(uint32_t unused);
int __cdecl midi_timing(void);
uint32_t *__stdcall AudioPrepareMidiData(const uint8_t *data, uint32_t size, uint32_t *out);

/* the engine's allocator (utils.c) */
void *__stdcall winrealloc(const void *p, unsigned long size, int zero);
void *__stdcall winfree(const void *p);

#endif
