/**
 * \example ex.04.memalloc/main.cpp
 * Example demonstrating non-collective
 * global memory allocation.
 */

#include <unistd.h>
#include <iostream>
#include <libdash.h>

#define SIZE 10

using namespace std;

int main(int argc, char* argv[])
{
  dash::init(&argc, &argv);

  auto myid = dash::myid();
  auto size = dash::size();

  using pointer_t = typename dash::Array<int>::pointer;

  dash::Array< pointer_t > arr(size);

  arr[myid] = dash::memalloc<int>(arr.globmem(), SIZE);

  for (int i = 0; i < SIZE; i++) {
    pointer_t ptr = arr[myid];
    ptr[i] = myid;
  }

  dash::barrier();

  cout << myid << ": ";
  for (int i = 0; i < SIZE; i++) {
    pointer_t ptr = arr[(myid+1) % size];
    cout << (int)ptr[i] << " ";
  }
  cout << endl;

  dash::barrier();

  dash::finalize();
}
