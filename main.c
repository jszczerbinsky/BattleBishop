#include "main.h"

#include <stdlib.h>
#include <time.h>

void Startpos(Board *b)
{
  b->turn                 = WHITE;
  b->piece[WHITE][PAWN]   = 0xff00;
  b->piece[BLACK][PAWN]   = 0xff000000000000;
  b->piece[WHITE][KNIGHT] = 0x42;
  b->piece[BLACK][KNIGHT] = 0x4200000000000000;
  b->piece[WHITE][ROOK]   = 0x81;
  b->piece[BLACK][ROOK]   = 0x8100000000000000;
  b->piece[WHITE][QUEEN]  = 0x8;
  b->piece[BLACK][QUEEN]  = 0x800000000000000;
  b->piece[WHITE][BISHOP] = 0x24;
  b->piece[BLACK][BISHOP] = 0x2400000000000000;
  b->piece[WHITE][KING]   = 0x10;
  b->piece[BLACK][KING]   = 0x1000000000000000;

  b->pieces_of[WHITE] = 0xffff;
  b->pieces_of[BLACK] = 0xffff000000000000;

  b->all_pieces = 0xffff00000000ffff;

  b->ep_possible = 0;

  b->moves_count = 0;
}

int main()
{
  Board b;

  Startpos(&b);

  Generate(&b, WHITE, GEN_ALL);
  /*srand(time(NULL));

  const BB  blockers = 0x20000102000;
  const int n        = 27;

  printf("0x%llx\n", precomp_bishop_moves[n][(blockers * precomp_bishop_magic[n]) >> (64 - 9)]);*/

  /*
      const BB  blockers = 0x40400100000;
      const int n        = 18;

      printf("0x%llx\n", precomp_bishop_moves[n][(blockers * precomp_bishop_magic[n]) >> (64 -
      12)]);*/
}
