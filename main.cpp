#include <iostream>
#include <cstdlib>
#include "createCSR.cpp"
using namespace std;

void bfs(CSR *adj, int s) // n=no of nodes s= starting node
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
    int *parent = (int *)calloc(n, sizeof(int));
    int *queue = (int *)calloc(n, sizeof(int));
    int front = -1, rear = -1, start, end, j;

    for (int i = 0; i < n; i++)
    {
        parent[i] = level[i] = -1;
    }

    level[s] = 1;
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
                parent[j] = w;
                queue[++rear] = j;
            }
        }
    }

    // cout << "Parent: " << endl;
    // for (int i = 0; i < n; i++)
    // {
    //     cout << parent[i] << "\t";
    // }
    // cout << endl;
    // cout << "level: " << endl;
    // for (int i = 0; i < n; i++)
    // {
    //     cout << level[i] << "\t";
    // }
    // cout << endl;

    cout << endl;
    cout << "-----------------  PATH  ------------------" << endl;
    for (int i = 0; i < n; i++)
    {
        // printf("\n%d -> %d : %d ; Path = ", s, i, level[i]);
        cout << s << " -> " << i << " : " << level[i] - 1 << " ; Path = ";
        if (i != s)
        {
            w = i;
            cout << w << " ---> ";
            while (parent[w] != -1)
            {
                w = parent[w];
                cout << w;
                if (parent[w] != -1)
                {
                    cout << " ---> ";
                }
            }
        }
        cout << endl;
    }
    free(level);
    free(queue);
    free(parent);
}

int main(int argc, char const *argv[])
{
    CSR *ptr = createCSR("data.txt");
    cout << "No of vertices: " << ptr->v_count << endl;
    cout << "No of Edges: " << ptr->e_count << endl;
    bfs(ptr, 0);
    return 0;
}
