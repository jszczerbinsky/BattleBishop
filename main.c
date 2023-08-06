#include "main.h"

#include <stdlib.h>
#include <time.h>

int main(int argc, char *argv[])
{
  Board b;
  // Startpos(&b);
  FEN(&b, argv[1]);

  clock_t start = clock();
  Search(&b);
  clock_t end = clock();

  printf("Total time: %ld\n", (end - start) / CLOCKS_PER_SEC);
}
