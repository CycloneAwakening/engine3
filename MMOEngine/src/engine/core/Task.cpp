#include "Core.h"

#include "engine/db/ObjectDatabaseManager.h"

#include "engine/stm/TransactionalMemoryManager.h"

#ifdef CXX11_COMPILER
#include <chrono>
#endif


Task::Task() : PriorityQueueEntry() {
	initialize();
}

Task::Task(uint64 mtime) : PriorityQueueEntry() {
	initialize();

	nextExecutionTime.addMiliTime(mtime);
}

Task::Task(Time& time) : PriorityQueueEntry() {
	initialize();

	nextExecutionTime = time;
}

Task::~Task() {
	assert(!isQueued());

#ifdef TRACE_TASKS
	if (scheduleTrace != nullptr) {
		delete scheduleTrace;
		scheduleTrace = nullptr;
	}
#endif
}

void Task::initialize() {
	taskManager = Core::getTaskManager();

	priority = 3;
	period = 0;

#ifdef TRACE_TASKS
	scheduleTrace = nullptr;
#endif

#ifdef COLLECT_TASKSTATISTICS
	lastElapsedTime = 0;
	statsSampleRate = 1;
#endif
}

void Task::execute() {
	taskManager->executeTask(this);
}

void Task::executeInThread() {
	doExecute();
}

void Task::doExecute() {
#ifdef WITH_STM
	//Logger::console.info("executing " + TypeInfo<Task>::getClassName(this), true);

	try {
		int commitAttempts = 0;

		Reference<engine::stm::Transaction*> transaction;

		while (true) {
			transaction = TransactionalMemoryManager::instance()->startTransaction();
			//engine::stm::Transaction* transaction = engine::stm::Transaction::currentTransaction();

			++commitAttempts;

			if (transaction->start(this) && transaction->commit()) {
				break;
			} else {
				Thread* thisThread = Thread::getCurrentThread();

				if (thisThread != nullptr && taskManager->getSerialWorker() != thisThread) {
					taskManager->retryTaskInSerial(this);

					TransactionalMemoryManager::instance()->retryTransaction();

					return;
				}
			}

			if (commitAttempts > 1)
				TransactionalMemoryManager::instance()->retryTransaction();
		}

		/*if (transaction->getReferenceCount() > 1)
			Logger::console.info("refs left of transaction " + String::valueOf(transaction->getReferenceCount()), true);*/

	} catch (Exception& e) {
		Logger::console.error("exception caught while running a task");
		e.printStackTrace();
	} catch (TransactionAbortedException& e) {
		Logger::console.error("in TASK");
	}
#else
	ObjectDatabaseManager::instance()->startLocalTransaction();

#ifdef COLLECT_TASKSTATISTICS
#ifdef CXX11_COMPILER
	auto start = std::chrono::high_resolution_clock::now();
#else
	Timer executionTimer;

	executionTimer.start();
#endif
#endif

	try {
		run();
	} catch (Exception& e) {
		Logger::console.error("exception caught while running a task");
		e.printStackTrace();
	} catch (...) {
		Logger::console.error("unreported exception caught while running a task");
	}

#ifdef COLLECT_TASKSTATISTICS
#ifdef CXX11_COMPILER
	auto end = std::chrono::high_resolution_clock::now();

	lastElapsedTime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
#else
	lastElapsedTime = executionTimer.stop();
#endif
#endif

	ObjectDatabaseManager::instance()->commitLocalTransaction();
#endif
}

bool Task::isScheduled() {
	return taskManager->isTaskScheduled(this);
}

bool Task::cancel() {
	return taskManager->cancelTask(this);
}

bool Task::cancelNonTransactionally() {
#ifdef WITH_STM
	return TransactionalMemoryManager::instance()->getTaskManager()->getTaskManagerImpl()->cancelTask(this);
#else
	return cancel();
#endif
}

void Task::schedule(uint64 delay) {
	taskManager->scheduleTask(this, delay);
}

void Task::scheduleInIoScheduler(uint64 delay) {
	taskManager->scheduleIoTask(this, delay);
}

void Task::scheduleInIoScheduler(Time& time) {
	taskManager->scheduleIoTask(this, time);
}

void Task::scheduleNonTransactionally(uint64 delay) {
#ifdef WITH_STM
	TransactionalMemoryManager::instance()->getTaskManager()->getTaskManagerImpl()->scheduleTask(this, delay);
#else
	schedule(delay);
#endif

}

void Task::schedule(Time& time) {
	taskManager->scheduleTask(this, time);
}

void Task::schedulePeriodic(uint64 delay, uint64 period) {
	setPeriod(period);

	taskManager->scheduleTask(this, delay);
}

void Task::schedulePeriodic(Time& time, uint64 period) {
	setPeriod(period);

	taskManager->scheduleTask(this, time);
}

void Task::reschedule(uint64 delay) {
	taskManager->rescheduleTask(this, delay);
}

void Task::rescheduleInIoScheduler(uint64 delay) {
	taskManager->rescheduleIoTask(this, delay);
}

void Task::rescheduleInIoScheduler(Time& time) {
	taskManager->rescheduleIoTask(this, time);
}

void Task::rescheduleNonTransactionally(uint64 delay) {
#ifdef WITH_STM
	TransactionalMemoryManager::instance()->getTaskManager()->getTaskManagerImpl()->rescheduleTask(this, delay);
#else
	reschedule(delay);
#endif
}

void Task::reschedule(Time& time) {
	taskManager->rescheduleTask(this, time);
}

