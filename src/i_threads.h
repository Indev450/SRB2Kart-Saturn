// SONIC ROBO BLAST 2 KART
//-----------------------------------------------------------------------------
// Copyright (C) 2020-2023 by James R.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  i_threads.h
/// \brief Multithreading abstraction

#ifdef HAVE_THREADS

#ifndef I_THREADS_H
#define I_THREADS_H

#include "doomtype.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*I_ThreadFn)(void *userdata);

typedef void * I_Mutex;
typedef void * I_Cond;

#ifdef __linux__
typedef long unsigned int thread_handle_t;
#else
typedef void *thread_handle_t;
#endif

void      I_StartThreads (void);
void      I_StopThreads  (void);

FUNCWARNRV
int       I_SpawnThread (const char *name, I_ThreadFn, void *userdata);

/* check in your thread whether to return early */
int       I_ThreadIsStopped (void);

void      I_LockMutex      (I_Mutex *);
void      I_UnlockMutex    (I_Mutex);

void      I_HoldCond       (I_Cond *, I_Mutex);

void      I_WakeOneCond   (I_Cond *);
void      I_WakeAllCond   (I_Cond *);

thread_handle_t I_GetCurrentThread(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif/*I_THREADS_H*/

#endif/*HAVE_THREADS*/
