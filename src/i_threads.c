// SONIC ROBO BLAST 2 KART
//-----------------------------------------------------------------------------
// Copyright (C) 2020-2023 by James R.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  i_threads.c
/// \brief Multithreading abstraction

#ifdef HAVE_THREADS
#if defined (__unix__) || defined(UNIXCOMMON)

#include <pthread.h>

#include "i_threads.h"
#include "doomdef.h"
#include "doomtype.h"

typedef struct thread_s thread_t;

struct thread_s
{
	thread_t *next;
	void *userdata;
	I_ThreadFn func;
	pthread_t thread;
};

// we use a linked list to avoid moving memory blocks when allocating new threads.
static thread_t *thread_list;
static pthread_mutex_t thread_lock = PTHREAD_MUTEX_INITIALIZER;

static void *HandleThread(void *data)
{
	thread_t *thread = data;
	thread->func(thread->userdata);

	pthread_mutex_lock(&thread_lock);
	thread->func = NULL;
	pthread_mutex_unlock(&thread_lock);
	return NULL;
}

FUNCWARNRV
int I_SpawnThread(const char *name, I_ThreadFn entry, void *userdata)
{
	thread_t *thread;
	(void)name;

	pthread_mutex_lock(&thread_lock);
	thread = thread_list;
	while (thread != NULL)
	{
		if (thread->func == NULL)
		{
			// join with the exited thread to release it's resources.
			pthread_join(thread->thread, NULL);
			break;
		}
		thread = thread->next;
	}
	if (thread == NULL)
	{
		thread = malloc(sizeof(thread_t));
		if (thread == NULL)
			return false;
		thread->next = thread_list;
		thread_list = thread;
	}

	thread->func = entry;
	thread->userdata = userdata;
	int err = pthread_create(&thread->thread, NULL, HandleThread, thread);
	pthread_mutex_unlock(&thread_lock);
	return err == 0;
}

int I_ThreadIsStopped(void)
{
	thread_t *thread;
	pthread_mutex_lock(&thread_lock);
	thread = thread_list;
	while (thread != NULL)
	{
		if (thread->func != NULL)
		{
			pthread_mutex_unlock(&thread_lock);
			return false;
		}
		thread = thread->next;
	}
	pthread_mutex_unlock(&thread_lock);
	return true;
}

void I_StartThreads(void)
{
}

void I_StopThreads(void)
{
	thread_t *thread = thread_list;
	while (thread != NULL)
	{
		// join with all threads here, since finished threads haven't been awaited yet.
		pthread_join(thread->thread, NULL);
		thread = thread->next;
	}
}

void I_LockMutex(I_Mutex *anchor)
{
	pthread_mutex_lock(&thread_lock);
	if (*anchor == NULL)
	{
		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);

		// SRB2 relies on lock recursion, so we need a mutex configured for that.
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
		*anchor = malloc(sizeof(pthread_mutex_t));
		pthread_mutex_init(*anchor, &attr);
		pthread_mutexattr_destroy(&attr);
	}
	pthread_mutex_unlock(&thread_lock);
	pthread_mutex_lock(*anchor);
}

void I_UnlockMutex(I_Mutex id)
{
	pthread_mutex_unlock(id);
}

void I_HoldCond(I_Cond *condAnchor, I_Mutex mutexId)
{
	I_Assert(mutexId != NULL);
	pthread_mutex_lock(&thread_lock);
	if (*condAnchor == NULL)
	{
		*condAnchor = malloc(sizeof(pthread_cond_t));
		pthread_cond_init(*condAnchor, NULL);
	}
	pthread_mutex_unlock(&thread_lock);
	pthread_cond_wait(*condAnchor, mutexId);
}

void I_WakeOneCond(I_Cond *anchor)
{
	pthread_mutex_lock(&thread_lock);
	if (*anchor == NULL)
	{
		*anchor = malloc(sizeof(pthread_cond_t));
		pthread_cond_init(*anchor, NULL);
	}
	pthread_mutex_unlock(&thread_lock);
	pthread_cond_signal(*anchor);
}

void I_WakeAllCond(I_Cond *anchor)
{
	pthread_mutex_lock(&thread_lock);
	if (*anchor == NULL)
	{
		*anchor = malloc(sizeof(pthread_cond_t));
		pthread_cond_init(*anchor, NULL);
	}
	pthread_mutex_unlock(&thread_lock);
	pthread_cond_broadcast(*anchor);
}

thread_handle_t I_GetCurrentThread(void)
{
	return pthread_self();
}
#elif defined (_WIN32)
#include <windows.h>

#include "i_threads.h"
#include "doomdef.h"
#include "doomtype.h"

typedef struct thread_s thread_t;

struct thread_s
{
	thread_t *next;
	void *userdata;
	I_ThreadFn func;
	HANDLE thread;
	DWORD threadId;
};

// we use a linked list to avoid moving memory blocks when allocating new threads.
static thread_t *thread_list;
static CRITICAL_SECTION thread_lock;

static DWORD __stdcall HandleThread(void *data)
{
	thread_t *thread = data;
	thread->func(thread->userdata);

	EnterCriticalSection(&thread_lock);
	thread->func = NULL;
	LeaveCriticalSection(&thread_lock);
	return 0;
}

FUNCWARNRV
int I_SpawnThread(const char *name, I_ThreadFn entry, void *userdata)
{
	thread_t *thread;
	(void)name;
	EnterCriticalSection(&thread_lock);
	thread = thread_list;
	while (thread != NULL)
	{
		if (thread->func == NULL)
		{
			CloseHandle(thread->thread);
			break;
		}
		thread = thread->next;
	}
	if (thread == NULL)
	{
		thread = malloc(sizeof(thread_t));
		if (thread == NULL)
			return false;
		thread->next = thread_list;
		thread_list = thread;
	}

	thread->func = entry;
	thread->userdata = userdata;
	thread->thread = CreateThread(NULL, 0, HandleThread, thread, 0, &thread->threadId);
	LeaveCriticalSection(&thread_lock);
	return thread->thread != NULL;
}

int I_ThreadIsStopped(void)
{
	thread_t *thread;
	EnterCriticalSection(&thread_lock);
	thread = thread_list;
	while (thread != NULL)
	{
		if (thread->func != NULL)
		{
			LeaveCriticalSection(&thread_lock);
			return false;
		}
		thread = thread->next;
	}
	LeaveCriticalSection(&thread_lock);
	return true;
}

void I_StartThreads(void)
{
	InitializeCriticalSection(&thread_lock);
}

void I_StopThreads(void)
{
	thread_t *thread = thread_list;
	while (thread != NULL)
	{
		WaitForSingleObject(thread->thread, INFINITE);
		CloseHandle(thread->thread);
		thread = thread->next;
	}
	DeleteCriticalSection(&thread_lock);
}

void I_LockMutex(I_Mutex *anchor)
{
	EnterCriticalSection(&thread_lock);
	if (*anchor == NULL)
	{
		*anchor = malloc(sizeof(CRITICAL_SECTION));
		InitializeCriticalSection(*anchor);
	}
	LeaveCriticalSection(&thread_lock);
	EnterCriticalSection(*anchor);
}

void I_UnlockMutex(I_Mutex id)
{
	LeaveCriticalSection(id);
}

void I_HoldCond(I_Cond *condAnchor, I_Mutex mutexId)
{
	I_Assert(mutexId != NULL);
	EnterCriticalSection(&thread_lock);
	if (*condAnchor == NULL)
	{
		*condAnchor = malloc(sizeof(CONDITION_VARIABLE));
		InitializeConditionVariable(*condAnchor);
	}
	LeaveCriticalSection(&thread_lock);
	SleepConditionVariableCS(*condAnchor, mutexId, INFINITE);
}

void I_WakeOneCond(I_Cond *anchor)
{
	EnterCriticalSection(&thread_lock);
	if (*anchor == NULL)
	{
		*anchor = malloc(sizeof(CONDITION_VARIABLE));
		InitializeConditionVariable(*anchor);
	}
	LeaveCriticalSection(&thread_lock);
	WakeConditionVariable(*anchor);
}

void I_WakeAllCond(I_Cond *anchor)
{
	EnterCriticalSection(&thread_lock);
	if (*anchor == NULL)
	{
		*anchor = malloc(sizeof(CONDITION_VARIABLE));
		InitializeConditionVariable(*anchor);
	}
	LeaveCriticalSection(&thread_lock);
	WakeAllConditionVariable(*anchor);
}

void *I_GetCurrentThread(void)
{
	return GetCurrentThread();
}
#else
#include "i_threads.h"

FUNCWARNRV
int I_SpawnThread(const char *name, I_ThreadFn entry, void *userdata)
{
	(void)name;
	entry(userdata);
	return true;
}

int I_ThreadIsStopped(void)
{
	return true;
}

void I_StartThreads(void)
{
}

void I_StopThreads(void)
{
}

void I_LockMutex(I_Mutex *anchor)
{
	(void)anchor;
}

void I_UnlockMutex(I_Mutex id)
{
	(void)id;
}

void I_HoldCond(I_Cond *condAnchor, I_Mutex mutexId)
{
	(void)condAnchor;
	(void)mutexId;
}

void I_WakeOneCond(I_Cond *anchor)
{
	(void)anchor;
}

void I_WakeAllCond(I_Cond *anchor)
{
	(void)anchor;
}

void *I_GetCurrentThread(void)
{
	return NULL;
}
#endif


#endif // HAVE_THREADS
