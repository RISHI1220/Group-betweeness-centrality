#include <iostream>
#include <cstdlib>
#include "createCSR.cpp"
using namespace std;

int main(int argc, char const *argv[])
{
    CSR *ptr = createCSR("data.txt");
    cout << "No of vertices: " << ptr->v_count << endl;
    cout << "No of Edges: " << ptr->e_count << endl;
    return 0;
}
