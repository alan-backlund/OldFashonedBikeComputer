/*
 * monitor.hpp
 *
 *  Created on: May 4, 2023
 *      Author: alan
 */

#ifndef INC_MONITOR_HPP_
#define INC_MONITOR_HPP_

#include <stdint.h>

#define timeInterval_t	uint32_t
#define NEVER   UINT32_MAX

typedef struct {
	char* name;
	timeInterval_t (*fn)();
	timeInterval_t delay;
	bool runNow;
} task_t;

class cMonitor {
public:
	void init(task_t* table, const int size);
	void run();
	void schedule(char* name, timeInterval_t delay);
	void scheduleIrq(char* name);
	void scheduleIrq(char* name, timeInterval_t delay);
private:
	task_t* TasksTable;
	int TasksTableSize;
	volatile bool WakeNow;
};

#endif /* INC_MONITOR_HPP_ */
