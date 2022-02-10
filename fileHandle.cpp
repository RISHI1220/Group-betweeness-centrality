#include <iostream>
#include <cstdlib>
#include <fstream>
using namespace std;

void sort(int *from, int *to, int n)
{
    int temp, temp2;
    for (int i = 0; i < n - 1; i++)
    {
        for (int j = i + 1; j < n; j++)
        {
            if (from[i] > from[j])
            {
                temp = from[i];
                from[i] = from[j];
                from[j] = temp;

                temp2 = to[i];
                to[i] = to[j];
                to[j] = temp2;
            }
        }
    }
}

void fileHandle(string str)
{
    ifstream fin(str);
    ofstream fout("finalData.txt");

    int v_count, e_count, left, right;
    fin >> v_count >> e_count;

    int *from = (int *)calloc(2 * e_count, sizeof(int));
    int *to = (int *)calloc(2 * e_count, sizeof(int));

    int count = 0;
    while (fin.eof() == 0)
    {
        fin >> left;
        fin.ignore();
        fin >> right;

        from[count] = left;
        to[count] = right;
        count++;
    }
    sort(from, to, 2 * e_count);

    fout << v_count << endl;
    fout << e_count << endl;
    for (int i = 0; i < 2 * e_count; i++)
    {
        fout << from[i] << "," << to[i] << endl;
    }
    fin.close();
    fout.close();
    free(from);
    free(to);
}

int main()
{
    cout << "Enter file name: " << endl;
    string str;
    cin >> str;
    fileHandle(str);
    cout << "Data sorted at finalData.txt" << endl;
    return 0;
}
