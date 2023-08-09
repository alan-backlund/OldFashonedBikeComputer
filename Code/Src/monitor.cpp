/*
 * monitor.cpp
 *
 *  Created on: May 4, 2023
 *      Author: alan
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "stm32l0xx_hal.h"

#include "monitor.hpp"


void cMonitor::init(task_t* table, const int size)
{
	TasksTable = table;
	TasksTableSize = size;
	WakeNow = false;
}


// Run until nothing left to do
// then return, expecting to be called again
// when an interrupt occurs which (may have) called
// schedule
void cMonitor::run()
{
	do {
		timeInterval_t minDelay = NEVER;

		bool rerun = false;
		for (int i=0; i<TasksTableSize; i++) {
			task_t* task = &TasksTable[i];
			// read and reset runNow flag with irq disabled
			__disable_irq();
			bool run = task->runNow;
			task->runNow = false;
			__enable_irq();
			// run each task with delay == 0
			if ((task->delay == 0) || run) {
				task->delay = (*task->fn)();
				rerun = rerun | task->runNow;
			}
			// track minimum delay
			if (task->delay < minDelay)
				minDelay = task->delay;
		}
		if (rerun)
			continue;
		if (minDelay == NEVER)
				break;

		uint32_t start = HAL_GetTick();
#if 0
		HAL_Delay(minDelay);
#else
		// MaybeDo Go to sleep for short e.g. 10ms intervals
		// Based on HAL_Delay()
		uint32_t tickstart = HAL_GetTick();
		uint32_t wait = minDelay;

		/* Add a freq to guarantee minimum wait */
		if (wait < HAL_MAX_DELAY)
			wait += (uint32_t)(uwTickFreq);

		while (((HAL_GetTick() - tickstart) < wait) && (!WakeNow))
			;
		WakeNow = false;
#endif
		uint32_t sleepTime = HAL_GetTick() - start;

		for (int i=0; i<TasksTableSize; i++) {
			task_t* task = &TasksTable[i];
			if (task->delay != NEVER) {
				if (task->delay >= sleepTime)
					task->delay -= sleepTime;
				else
					task->delay = 0;
			}
		}

	} while (true);
}

void cMonitor::schedule(char* name, timeInterval_t delay)
{
	int i;
	for (i=0; i<TasksTableSize; i++)
		if (strcmp(name, TasksTable[i].name) == 0)
			break;
	if (i == TasksTableSize)
		return;
	if (delay > 0)
		TasksTable[i].delay = delay;
	else {
		TasksTable[i].runNow = true;
		WakeNow = true;
	}
}

void cMonitor::scheduleIrq(char* name, timeInterval_t delay)
{
	int i;
	for (i=0; i<TasksTableSize; i++)
		if (strcmp(name, TasksTable[i].name) == 0)
			break;
	if (i == TasksTableSize)
		return;
	if (delay > 0)
		TasksTable[i].delay = delay;
	else
		TasksTable[i].runNow = true;
	WakeNow = true;
}

void cMonitor::scheduleIrq(char* name)
{
	int i;
	for (i=0; i<TasksTableSize; i++)
		if (strcmp(name, TasksTable[i].name) == 0)
			break;
	if (i == TasksTableSize)
		return;
	TasksTable[i].runNow = true;
	WakeNow = true;
}



