#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <queue>
#include <stack>
#include <list>
#include "createCSR.cpp"
using namespace std;

float gbc;
int *group;
int group_size;

int main()
{
    CSR *csr = createCSR("data4.txt");
    cout << "No of vertices: " << csr->v_count << endl;
    cout << "No of Edges: " << csr->e_count << endl;

    cout << "Enter the length of the group: ";
    cin >> group_size;
    group = (int *)calloc(group_size, sizeof(int));
    gbc = 0;

    cout << "Enter vertices for the group: " << endl;
    int temp;
    for (int i = 0; i < group_size; i++)
    {
        cin >> temp;
        group[temp] = 1;
    }

    for (int s = 0; s < csr->v_count; s++) // adding task to taskQueue
    {
        if (s > csr->v_count || s < 0)
        {
            cout << "-----Starting node out of bounds-----" << endl;
            cout << "EXITING..." << endl;
            exit(0);
        }

        int w, top;
        int n = csr->v_count;
        int *level = (int *)calloc(n, sizeof(int));
        list<int> parent[n];
        int *nos = (int *)calloc(n, sizeof(int)); // to calc no of total shortest path from root
        float *back = (float *)calloc(n, sizeof(float));
        queue<int> queue;
        stack<int> stack;
        int start, end, j;

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
            start = csr->list_v[w];
            if (w == (n - 1))
            {
                end = csr->e_count;
            }
            else
            {
                end = csr->list_v[w + 1];
            }

            for (int i = start; i < end; i++)
            {
                j = csr->list_e[i];
                if (level[j] == -1)
                {
                    level[j] = level[w] + 1;
                    queue.push(j);
                }
                if ((level[j] - 1) == (level[w]))
                {
                    nos[j] = nos[j] + nos[w];
                    parent[j].push_back(w);
                }
            }
        }

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
                gbc = gbc + back[top];
            }
        }

        // for (int v = 0; v < n; v++)
        // {
        //     if (v != s)
        //     {
        //         bwc[v] = bwc[v] + back[v];
        //     }
        // }

        free(level);
        free(nos);
        free(back);
        parent->clear();
    }

    printf("\n-------Group Betweeness centrality-------\n");

    gbc = gbc / 2;
    printf("GBC: %f\n", gbc);
    free(group);
    return 0;
}