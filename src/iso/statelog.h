/* Test aid: per-tick hashes of the units' state (statelog.c) */
#ifndef DOMINION_ISO_STATELOG_H
#define DOMINION_ISO_STATELOG_H

void statelog_install(void);
void statelog_frame(void); /* at the start of every displayed frame */

#endif
