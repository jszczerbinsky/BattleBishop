#include "main.h"

#include <stdlib.h>

int main(int argc, char *argv[])
{
  Board b;
  // Startpos(&b);
  FEN(&b, argv[1]);

  Search(&b);
}
