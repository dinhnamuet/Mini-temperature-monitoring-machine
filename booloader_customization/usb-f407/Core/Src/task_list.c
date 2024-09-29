#include "task_list.h"
#include <stdlib.h>
#include <string.h>

/**
  * @brief  free memory of queue
  * @param  queue: queue to free
  * @retval none
*/
void free_task_list(struct task_queue *queue) {
    free(queue->task);
	queue->task = NULL;
}
/**
  * @brief  check queue is empty
  * @param  queue: queue to check
  * @retval 1: queue is empty, 0: data is avaiable
*/
int queue_is_empty(struct task_queue *queue) {
	return (atomic_load(&queue->n_tasks) == 0);
}
/**
  * @brief  put task to current queue, if the queue is already full, task will be 
  * put at head of queue if head task was handled
  * @param  task: task to put
  * @retval 0: success, -1: failed
*/
int put_task_to_queue(struct task_queue *queue, struct task_struct task) {
	if ((atomic_load(&queue->n_tasks)) == queue->queue_size) {
		return -1; /* queue is full */
	} else {
		queue->task[queue->write_index] = task;
		queue->write_index = (queue->write_index + 1) % queue->queue_size;
		atomic_fetch_add(&queue->n_tasks, 1);
	    return 0;
	}
}

/**
  * @brief  Get head of task list
  * @param  queue: queue
  * @param  none
  * @retval head of task list
*/
struct task_struct *get_new_task(struct task_queue *queue) {
	if (atomic_load(&queue->n_tasks) == 0) {
		return NULL;
	} else {
		int index = queue->read_index;
		queue->read_index = (queue->read_index + 1) % queue->queue_size;
		atomic_fetch_sub(&queue->n_tasks, 1);
		return &queue->task[index];
	}
}

/**
  * @brief  Initialize task queue
  * @param  queue: queue to init
  * @param  size: queue size (maximum of task to be executed)
  * @retval 0: success, -1: failed
*/
int init_queue(struct task_queue *queue, uint32_t size) {
	queue->task = (struct task_struct *)calloc(size, sizeof(struct task_struct));
	if (!queue->task) {
		return -1;
	}
	queue->read_index = 0;
	queue->write_index = 0;
	atomic_init(&queue->n_tasks, 0);
	queue->queue_size = size;
	return 0;
}
