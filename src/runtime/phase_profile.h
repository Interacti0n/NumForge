#ifndef NUMFORGE_PHASE_PROFILE_H
#define NUMFORGE_PHASE_PROFILE_H

typedef enum NumForgeProfilePhase
{
    NUMFORGE_PHASE_OTHER,
    NUMFORGE_PHASE_PARSE,
    NUMFORGE_PHASE_EVALUATE,
    NUMFORGE_PHASE_FORMAT,
    NUMFORGE_PHASE_CONVERT,
    NUMFORGE_PHASE_ROUND,
    NUMFORGE_PHASE_COMPOSE,
    NUMFORGE_PHASE_SERIALIZE,
    NUMFORGE_PHASE_SEND,
    NUMFORGE_PHASE_NORMALIZE,
    NUMFORGE_PHASE_COUNT
} NumForgeProfilePhase;

/* Per-thread exclusive phases. Inactive hooks do not read the clock or allocate.
 * A request owns reset/stop; nested scopes restore the preceding phase. */
void numforge_profile_reset(void);
void numforge_profile_stop(void);
int numforge_profile_complete(void);
int numforge_profile_active(void);
NumForgeProfilePhase numforge_profile_enter(NumForgeProfilePhase phase);
void numforge_profile_leave(NumForgeProfilePhase previous);
double numforge_profile_seconds(NumForgeProfilePhase phase);
#ifdef NUMFORGE_ENABLE_ALLOC_STATS
unsigned long long numforge_profile_process_peak(void);
#endif
#endif
