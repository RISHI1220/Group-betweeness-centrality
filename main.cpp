#include <pthread.h>
#include <iostream>
#include <cstdlib>
#include <Windows.h>
#include <semaphore.h>
#include <time.h>
#include "createCSR.cpp"
using namespace std;

#define THREAD_NUM 4 // No of threads

typedef struct Task
{
    void (*taskFunction)(CSR *, int);
    CSR *adj;
    int s;
} Task; // task structure to store task , adj -> csr matrix, s -> starting node( diff for each threads)

Task taskQueue[256]; // array of type task to store different tasks
int taskCount = 0;   // to keep count of running task

pthread_mutex_t mutexQueue; // queue mutex
pthread_cond_t condQueue;   // queue condition variable

void bfs(CSR *adj, int s)
{
    if (s > adj->v_count || s < 0)
    {
        cout << "-----Starting node out of bounds-----" << endl;
        cout << "EXITING..." << endl;
        exit(0);
    }

    int w;
    int n = adj->v_count;
    int *level = (int *)calloc(n, sizeof(int));
    int *queue = (int *)calloc(n, sizeof(int));
    int *nos = (int *)calloc(n, sizeof(int)); // to calc no of total shortest path from root
    int front = -1, rear = -1, start, end, j;

    for (int i = 0; i < n; i++)
    {
        level[i] = -1;
    }

    level[s] = 0;
    nos[s] = 1;
    queue[++rear] = s;

    while (front != rear)
    {
        w = queue[++front];
        start = adj->list_v[w];
        if (w == (n - 1))
        {
            end = adj->e_count;
        }
        else
        {
            end = adj->list_v[w + 1];
        }

        for (int i = start; i < end; i++)
        {
            j = adj->list_e[i];
            if (level[j] == -1)
            {
                level[j] = level[w] + 1;
                queue[++rear] = j;
            }
            if ((level[j] - 1) == (level[w]))
            {
                nos[j] = nos[j] + nos[w];
            }
        }
    }
    printf("-----------------  No. of shortest paths from node %d ------------------\n", s);
    for (int i = 0; i < n; i++)
    {
        printf("Node %d is %d\n", i, nos[i]);
    }

    free(level);
    free(queue);
    free(nos);
}

void executeTask(Task *task)
{
    task->taskFunction(task->adj, task->s); // executing task passed from threadPool
}

void submitTask(Task task) // to add task to taskQueue which is passed from main
{
    pthread_mutex_lock(&mutexQueue);
    taskQueue[taskCount] = task; // add task to task queue
    taskCount++;
    pthread_mutex_unlock(&mutexQueue);
    pthread_cond_signal(&condQueue); // signalling threadPool when a task is added
}

void *threadPool(void *args)
{
    while (1)
    {
        Task task;
        pthread_mutex_lock(&mutexQueue);

        while (taskCount == 0)
        {
            pthread_cond_wait(&condQueue, &mutexQueue); // condition variable to wait here until a task is added by submit task
        }

        task = taskQueue[0]; // picking task from taskQueue
        for (int i = 0; i < taskCount - 1; i++)
        {
            taskQueue[i] = taskQueue[i + 1]; // shifting all remaining task to first
            // 1 2 3 4 5
            // 2 3 4 5
        }
        taskCount--;

        pthread_mutex_unlock(&mutexQueue);
        executeTask(&task); // passing the top task extracted to be executed
    }
    return NULL;
}

int main(int argc, char const *argv[])
{
    CSR *csr = createCSR("data2.txt");
    cout << "No of vertices: " << csr->v_count << endl;
    cout << "No of Edges: " << csr->e_count << endl;

    pthread_t th[THREAD_NUM];
    pthread_mutex_init(&mutexQueue, NULL);
    pthread_cond_init(&condQueue, NULL);

    for (int i = 0; i < THREAD_NUM; i++)
    {
        pthread_create(&th[i], NULL, &threadPool, NULL); // creating the thread pool
    }

    for (int i = 0; i < csr->v_count; i++) // adding task to taskQueue
    {
        Task t;
        t.taskFunction = &bfs;
        t.adj = csr;
        t.s = i; // i represents each vertex , each vertex is being passed as argument
        submitTask(t);
    }

    for (int i = 0; i < THREAD_NUM; i++)
    {
        pthread_join(th[i], NULL);
    }

    pthread_mutex_destroy(&mutexQueue);
    pthread_cond_destroy(&condQueue);
    return 0;
}
