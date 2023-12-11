// SPDX-License-Identifier: BSD-3-Clause

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "log/log.h"
#include "os_graph.h"
#include "os_threadpool.h"
#include "utils.h"

#define NUM_THREADS 4

static int sum;
static os_graph_t *graph;
static os_threadpool_t *tp;
/* Define graph synchronization mechanisms. */
static pthread_mutex_t graph_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t graph_condition = PTHREAD_COND_INITIALIZER;

static void process_node(void *argument)
{
	/* TODO: Implement thread-pool based processing of graph. */
	os_node_t *node;

	pthread_mutex_lock(&graph_mutex);
	graph_task_arg_t *arg = (graph_task_arg_t *)argument;

	node = graph->nodes[arg->node_index];
	sum += node->info;
	graph->visited[arg->node_index] = DONE;
	pthread_mutex_unlock(&graph_mutex);
	for (unsigned int i = 0; i < node->num_neighbours; i++) {
		pthread_mutex_lock(&queue_mutex);
		if (graph->visited[node->neighbours[i]] == NOT_VISITED) {
			graph_task_arg_t *new_arg = malloc(sizeof(graph_task_arg_t));

			new_arg->node_index = node->neighbours[i];
			os_task_t *t = create_task(process_node, (void *)new_arg, free);

			graph->visited[new_arg->node_index] = DONE;
			enqueue_task(tp, t);
		}
		pthread_mutex_unlock(&queue_mutex);
	}
}

int main(int argc, char *argv[])
{
	FILE *input_file;
	os_task_t *t;
	graph_task_arg_t *arg = malloc(sizeof(graph_task_arg_t));

	arg->node_index = 0;
	if (argc != 2) {
		fprintf(stderr, "Usage: %s input_file\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	input_file = fopen(argv[1], "r");
	DIE(input_file == NULL, "fopen");

	graph = create_graph_from_file(input_file);
	for (unsigned int i = 0; i < graph->num_nodes; i++)
		graph->visited[i] = NOT_VISITED;
	pthread_mutex_init(&graph_mutex, NULL);
	pthread_cond_init(&graph_condition, NULL);

	t = create_task(process_node, (void *)arg, free);
	tp = create_threadpool(NUM_THREADS);
	enqueue_task(tp, t);
	wait_for_completion(tp);
	destroy_threadpool(tp);

	printf("%d", sum);
	pthread_mutex_destroy(&graph_mutex);
	pthread_cond_destroy(&graph_condition);

	return 0;
}
