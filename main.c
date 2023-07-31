#include "main.h"

#include <stdlib.h>
#include <time.h>

typedef struct
{
  int total;
  int eps;
  int castles;
  int promotions;
  int captures;
} PerftResult;

static void perft(Board *b, int depth, PerftResult *result)
{
  if (depth == 0) return;

  Move move_list[MAX_MOVES];
  int  move_count;

  Generate(b, b->turn, GEN_ALL, move_list, &move_count);

  for (int i = 0; i < move_count; i++)
  {
    MakeMove(b, move_list[i]);

    if (!SquareAttackedBy(b, b->turn, ffsll(b->piece[!b->turn][KING]) - 1))
    {
      if (depth == 1)
      {
        result->total++;
        switch (GET_TYPE(move_list[i]))
        {
          case MOVE_TYPE_EP:
            result->eps++;
            break;
          case MOVE_TYPE_CASTLE_K:
            result->castles++;
            break;
          case MOVE_TYPE_CASTLE_Q:
            result->castles++;
            break;
          case MOVE_TYPE_CAPTURE:
            result->captures++;
            break;
          case MOVE_TYPE_PROMOTION:
            result->promotions++;
            break;
          case MOVE_TYPE_PROMOTION_WITH_CAPTURE:
            result->promotions++;
            result->captures++;
            break;
        }
      }
      perft(b, depth - 1, result);
    }
    UnmakeMove(b);
  }
}

static void runPerft(Board *b, int depth)
{
  PerftResult result;
  memset(&result, 0, sizeof(PerftResult));

  clock_t start = clock();
  perft(b, depth, &result);
  printf("Perft(%d):\ntotal:%d\neps:%d\ncaptures:%d\ncastles:%d\npromotions:%d\n\n\n", depth,
         result.total, result.eps, result.captures, result.castles, result.promotions);
  clock_t end = clock();

  double time = (double)(end - start) / CLOCKS_PER_SEC;
  long   nps  = result.total / time;

  printf("Nodes per second: %ld\n\n\n", nps);
}

static void runPerftDiv(Board *b, int depth)
{
  Move move_list[MAX_MOVES];
  int  move_count;

  Generate(b, b->turn, GEN_ALL, move_list, &move_count);

  int total = 0;

  for (int i = 0; i < move_count; i++)
  {
    MakeMove(b, move_list[i]);

    if (!SquareAttackedBy(b, b->turn, ffsll(b->piece[!b->turn][KING]) - 1))
    {
      PerftResult result;
      memset(&result, 0, sizeof(PerftResult));

      perft(b, depth - 1, &result);

      total += result.total;

      char buff[6];
      PrintMoveStr(buff, move_list[i]);
      printf("%d - %s: %d\n", i, buff, result.total);
    }

    UnmakeMove(b);
  }

  printf("\nNodes searched: %d\n", total);
}
int main(int argc, char *argv[])
{
  Board b;
  Startpos(&b);

  Move move_list[MAX_MOVES];
  int  move_count;

  runPerft(&b, atoi(argv[1]));
}
