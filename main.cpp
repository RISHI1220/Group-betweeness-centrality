#include <pthread.h>
#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <semaphore.h>
#include <queue>
#include <stack>
#include <list>
#include <chrono>
#include "createCSR.cpp"
using namespace std;

#define THREAD_NUM 8 // No of threads

typedef struct Task
{
    void (*taskFunction)(CSR *, int);
    CSR *adj;
    int s;
} Task; // task structure to store task , adj -> csr matrix, s -> starting node( diff for each threads)

queue<Task> taskQueue; // array of type task to store different tasks
float gbc;             // calculate gbc
int *group;            // to store the group
int group_size;        // size of the each or one group

pthread_mutex_t mutexQueue; // queue mutex
pthread_mutex_t mutexBwc;   // betweeness cantrality mutex
pthread_cond_t condQueue;   // queue condition variable

void bfs(CSR *adj, int s)
{
    if (s > adj->v_count || s < 0)
    {
        cout << "-----Starting node out of bounds-----" << endl;
        cout << "EXITING..." << endl;
        exit(0);
    }

    int w, top, start, end, j;
    int n = adj->v_count;
    list<int> parent[n];
    queue<int> queue;
    stack<int> stack;
    int *level = (int *)calloc(n, sizeof(int));
    int *nos = (int *)calloc(n, sizeof(int)); // to calc no of total shortest path from root
    float *back = (float *)calloc(n, sizeof(float));

    for (int i = 0; i < n; i++)
    {
        level[i] = -1;
    }

    level[s] = 0;
    nos[s] = 1;
    queue.push(s);

    while (!queue.empty())
    {
        w = queue.front();
        queue.pop();
        stack.push(w);
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
            // Path discovery
            j = adj->list_e[i];
            if (level[j] == -1)
            {
                level[j] = level[w] + 1;
                queue.push(j);
            }

            // Path counting
            if ((level[j] - 1) == (level[w]))
            {
                nos[j] = nos[j] + nos[w];
                parent[j].push_back(w);
            }
        }
    }

    // accumulation or backtracking of dependencies
    while (!stack.empty())
    {
        top = stack.top();
        stack.pop();

        for (auto v : parent[top])
        {
            int temp = back[top];
            if (group[top] == 1)
            {
                temp = 0;
            }
            back[v] = back[v] + (((float)nos[v] / (float)nos[top]) * (1 + temp));
        }
        if (group[top] == 1 && top != s)
        {
            pthread_mutex_lock(&mutexBwc);
            gbc = gbc + back[top];
            pthread_mutex_unlock(&mutexBwc);
        }
    }
    free(level);
    free(nos);
    free(back);
    parent->clear();
}

void executeTask(Task *task)
{
    task->taskFunction(task->adj, task->s); // executing task passed from threadPool
}

void submitTask(Task task) // to add task to taskQueue which is passed from main
{
    pthread_mutex_lock(&mutexQueue);
    taskQueue.push(task); // pushing task to taskQueue
    pthread_mutex_unlock(&mutexQueue);
    pthread_cond_signal(&condQueue); // signalling threadPool when a task is added
}

void *threadPool(void *args)
{
    while (!taskQueue.empty())
    {
        Task task;
        pthread_mutex_lock(&mutexQueue);

        while (taskQueue.empty())
        {
            pthread_cond_wait(&condQueue, &mutexQueue); // condition variable to wait here until a task is added by submit task
        }

        task = taskQueue.front(); // picking task from taskQueue
        taskQueue.pop();

        pthread_mutex_unlock(&mutexQueue);
        executeTask(&task); // passing the top task extracted to be executed
    }
    return NULL;
}

int main(int argc, char const *argv[])
{
    CSR *csr = createCSR("facebook.txt");
    cout << "No of vertices: " << csr->v_count << endl;
    cout << "No of Edges: " << csr->e_count << endl;

    cout << "Enter the length of the group: ";
    cin >> group_size;
    group = (int *)calloc(csr->v_count, sizeof(int));
    gbc = 0;

    double norm = ((csr->v_count - group_size) * (csr->v_count - group_size - 1));

    cout << "Enter vertices for the group: " << endl;
    for (int i = 0; i < group_size; i++)
    {
        int temp;
        cin >> temp;
        group[temp] = 1;
    }

    pthread_t th[THREAD_NUM];
    pthread_mutex_init(&mutexQueue, NULL);
    pthread_cond_init(&condQueue, NULL);
    pthread_mutex_init(&mutexBwc, NULL);

    auto start = chrono::high_resolution_clock::now();

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

    auto end = chrono::high_resolution_clock::now();
    chrono::duration<float> duration = end - start;
    printf("\n-------Group Betweeness centrality-------\n");

    gbc = gbc / 2;          // Rescaling due to bidirectional graph
    gbc = gbc * (1 / norm); // normalization

    printf("GBC: %f \n", gbc);
    cout << "Time taken to execute: " << duration.count() << "s" << endl;
    pthread_mutex_destroy(&mutexQueue);
    pthread_cond_destroy(&condQueue);
    pthread_mutex_destroy(&mutexBwc);
    free(group);
    return 0;
}
