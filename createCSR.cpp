/*-------------------------------------------------------------
Program description: API to read the graph in Compressed Sparse Row(CSR)
--------------------------------------------------------------*/
#include <iostream>
#include <cstdlib>
#include <fstream>
using namespace std;

struct CSR
{
    int v_count;
    int e_count;
    int *list_v;
    int *list_e;
};

CSR *createCSR(string str)
{
    CSR *csr;
    ifstream fin(str);
    csr = (CSR *)malloc(sizeof(CSR));
    if (csr == NULL)
    {
        cout << "Memory initilization failed" << endl;
        exit(0);
    }
    // cout << "Enter number of vertices: " << endl;
    // cin >> csr->v_count;
    fin >> csr->v_count;
    // cout << "Enter number of edges: " << endl;
    // cin >> csr->e_count;
    fin >> csr->e_count;

    csr->e_count = 2 * csr->e_count;

    csr->list_v = (int *)calloc(csr->v_count, sizeof(int));
    csr->list_e = (int *)calloc(csr->e_count, sizeof(int));

    if (csr->list_e == NULL || csr->list_v == NULL)
    {
        cout << "Memory initilization failed" << endl;
        exit(0);
    }

    int ep = 0, left, right, prev;

    // cout << "Enter edge:" << endl;
    // cin >> left >> right;
    fin >> left;
    fin.ignore();
    fin >> right;

    while (ep < csr->e_count && fin.eof() == 0)
    {
        prev = left;
        csr->list_v[left] = ep;
        while (prev == left && ep < csr->e_count && fin.eof() == 0)
        {
            csr->list_e[ep] = right;
            ep++;
            if (ep < csr->e_count && fin.eof() == 0)
            {
                // cin >> left >> right;
                fin >> left;
                fin.ignore();
                fin >> right;
            }
            // cout << "pe = " << pe << endl;
        }
    }

    // vertex and edge list output
    cout << "Vertex list / Row list : " << endl;
    for (int i = 0; i < csr->v_count; i++)
    {
        cout << csr->list_v[i] << "\t";
    }
    cout << endl;
    cout << "Edge list / Column list : " << endl;
    for (int i = 0; i < csr->e_count; i++)
    {
        cout << csr->list_e[i] << "\t";
    }
    cout << endl;
    fin.close();
    return csr;
}