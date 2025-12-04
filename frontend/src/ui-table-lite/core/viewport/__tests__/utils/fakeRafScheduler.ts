import type { RafScheduler, ScheduleOptions } from "../../../runtime/rafScheduler"

type SchedulerPriority = "high" | "normal" | "low"

type PendingTask = {
  id: number
  callback: () => void
  priority: SchedulerPriority
}

class FakeRafSchedulerImpl implements RafScheduler {
  private readonly queues: Record<SchedulerPriority, Map<number, PendingTask>> = {
    high: new Map(),
    normal: new Map(),
    low: new Map(),
  }

  private nextTaskId = 1
  private disposed = false

  schedule(callback: () => void, options: ScheduleOptions = {}): number {
    if (this.disposed) {
      return -1
    }
    const taskId = this.nextTaskId
    this.nextTaskId += 1

    if (options.immediate) {
      callback()
      return taskId
    }

    const priority = options.priority ?? "normal"
    this.queues[priority].set(taskId, { id: taskId, callback, priority })
    return taskId
  }

  cancel(taskId: number): void {
    (Object.keys(this.queues) as SchedulerPriority[]).forEach(priority => {
      this.queues[priority].delete(taskId)
    })
  }

  flush(): void {
    if (this.disposed) {
      return
    }
    while (this.totalPendingTasks()) {
      this.tickFrame()
    }
  }

  runNow(callback: () => void): void {
    if (this.disposed) {
      return
    }
    callback()
  }

  dispose(): void {
    if (this.disposed) {
      return
    }
    this.disposed = true
    ;(Object.keys(this.queues) as SchedulerPriority[]).forEach(priority => {
      this.queues[priority].clear()
    })
  }

  tickFrame(): void {
    if (this.disposed) {
      return
    }
    this.runPriority("high")
    this.runPriority("normal")
    this.runPriority("low")
  }

  totalPendingTasks(): number {
    return (Object.keys(this.queues) as SchedulerPriority[]).reduce((total, priority) => {
      return total + this.queues[priority].size
    }, 0)
  }

  private runPriority(priority: SchedulerPriority): void {
    const tasks = this.queues[priority]
    if (!tasks.size) {
      return
    }
    const callbacks = Array.from(tasks.values())
    tasks.clear()
    callbacks.forEach(({ callback }) => {
      callback()
    })
  }
}

export interface FakeRafSchedulerHandle {
  scheduler: RafScheduler
  tickFrame(): void
  pendingTasks(): number
}

export function createFakeRafScheduler(): FakeRafSchedulerHandle {
  const impl = new FakeRafSchedulerImpl()
  return {
    scheduler: impl,
    tickFrame: () => impl.tickFrame(),
    pendingTasks: () => impl.totalPendingTasks(),
  }
}
