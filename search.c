#include <math.h>
#include <stdio.h>

#include "main.h"

typedef struct
{
  Move plies[MAX_MOVES];
  int  plies_count;
} Variation;

static const int piece_value[6] = {
    100,  // Pawn
    300,  // Knight
    500,  // Rook
    900,  // Queen
    300,  // Bishop
};

static void printVariation(Variation *variation)
{
  for (int i = 0; i < variation->plies_count; i++)
  {
    char buff[6];
    PrintMoveStr(buff, variation->plies[i]);
    printf("%s ", buff);
  }
}

static int popcnt(BB b)
{
  int sq;
  int i = 0;
  while ((sq = ffsll(b)) != 0)
  {
    i++;
    b &= ~SQ_TO_BB(sq - 1);
  };
  return i;
}

float Evaluate(Board *b)
{
  int who2move[2] = {1, -1};

  int total = 0;

  for (int i = 0; i < 5; i++)
    total += (popcnt(b->piece[WHITE][i]) - popcnt(b->piece[BLACK][i])) * piece_value[i];

  return total * who2move[b->turn];
}

static void selectNextMove(int i, Move *move_list, int *order_score, int move_count)
{
  int min = i;
  for (int j = i + 1; j < move_count; j++)
    if (order_score[j] < order_score[min]) min = j;

  if (min != i)
  {
    Move tmp       = move_list[i];
    move_list[i]   = move_list[min];
    move_list[min] = tmp;
    ;
  }
}

static void assignMoveOrderScore(Move *move_list, int *order_score, int move_count,
                                 Variation *variation, Variation *pv)
{
  int  is_pv;
  Move pv_move;

  if (pv->plies_count == 0)
    is_pv = 0;
  else
  {
    is_pv = 1;
    for (int i = 0; i < variation->plies_count; i++)
      if (pv->plies[i] != variation->plies[i])
      {
        is_pv = 0;
        break;
      }
    if (is_pv) pv_move = pv->plies[variation->plies_count];
  }

  for (int i = 0; i < move_count; i++)
  {
    if (is_pv && move_list[i] == pv_move)
      order_score[i] = 10000;
    else
    {
      int score;

      switch (GET_TYPE(move_list[i]))
      {
        case MOVE_TYPE_EP:
          score = 1;
          break;
        case MOVE_TYPE_CAPTURE:
          score = GET_CAPTURED_PIECE(move_list[i]) - GET_PIECE(move_list[i]);
          break;
        case MOVE_TYPE_PROMOTION:
          score = GET_PROMOTION_PIECE(move_list[i]);
          break;
        case MOVE_TYPE_PROMOTION_WITH_CAPTURE:
          score = GET_CAPTURED_PIECE(move_list[i]) + GET_PROMOTION_PIECE(move_list[i]);
          break;
        default:
          score = 0;
          break;
      }

      order_score[i] = score;
    }
  }
}

float alphaBeta(Board *b, float alpha, float beta, int depthleft, Variation *variation,
                Variation *parent_pv, Variation *pv)
{
  Variation child_pv;
  child_pv.plies_count = 0;

  if (depthleft == 0)
  {
    float score            = Evaluate(b);
    parent_pv->plies_count = 0;
    return score;
  }

  // Only silents moves on leaves
  int move_type = /*depthleft == 1 ? GEN_SILENT :*/ GEN_ALL;

  Move move_list[MAX_MOVES];
  int  order_score[MAX_MOVES];
  int  move_count;

  Generate(b, b->turn, move_type, move_list, &move_count);

  assignMoveOrderScore(move_list, order_score, move_count, variation, pv);

  int legal_found = 0;

  for (int i = 0; i < move_count; i++)
  {
    selectNextMove(i, move_list, order_score, move_count);

    if (IsLegal(b, move_list[i]))
    {
      legal_found++;

      MakeMove(b, move_list[i]);
      variation->plies[variation->plies_count] = move_list[i];
      variation->plies_count++;

      float score = -alphaBeta(b, -beta, -alpha, depthleft - 1, variation, &child_pv, pv);

      variation->plies_count--;
      UnmakeMove(b);

      if (score >= beta) return score;
      if (score > alpha)
      {
        alpha = score;

        parent_pv->plies[0] = move_list[i];
        if (child_pv.plies_count > 0)
          memcpy(parent_pv->plies + 1, child_pv.plies, child_pv.plies_count * sizeof(Move));
        parent_pv->plies_count = child_pv.plies_count + 1;
      }
    }
  }

  if (legal_found == 0)
  {
    float score = 0;

    if (IsKingAttacked(b, b->turn)) score = -999999999.0;  // Infinity fails

    parent_pv->plies_count = 0;

    return score;
  }

  return alpha;
}

Move searchRoot(Board *b, int depthleft, Variation *parent_pv, Variation *pv)
{
  Variation variation;
  variation.plies_count = 0;

  Variation child_pv;
  child_pv.plies_count = 0;

  Move move_list[MAX_MOVES];
  int  order_score[MAX_MOVES];
  int  move_count;

  Generate(b, b->turn, GEN_ALL, move_list, &move_count);
  assignMoveOrderScore(move_list, order_score, move_count, &variation, pv);

  Move  best  = move_list[0];
  float alpha = -INFINITY;
  float beta  = INFINITY;

  for (int i = 0; i < move_count; i++)
  {
    selectNextMove(i, move_list, order_score, move_count);

    if (IsLegal(b, move_list[i]))
    {
      MakeMove(b, move_list[i]);
      variation.plies[variation.plies_count] = move_list[i];
      variation.plies_count++;

      float score = -alphaBeta(b, -beta, -alpha, depthleft - 1, &variation, &child_pv, pv);

      variation.plies_count--;
      UnmakeMove(b);

      if (score > alpha)
      {
        alpha = score;
        best  = move_list[i];

        parent_pv->plies[0] = move_list[i];
        if (child_pv.plies_count > 0)
          memcpy(parent_pv->plies + 1, child_pv.plies, child_pv.plies_count * sizeof(Move));
        parent_pv->plies_count = child_pv.plies_count + 1;
      }
    }
  }

  return best;
}

Move Search(Board *b)
{
  Variation pv;
  pv.plies_count = 0;

  for (int depth = 2; depth < 12; depth++)
  {
    Variation child_pv;
    child_pv.plies_count = 0;

    Move m = searchRoot(b, depth, &child_pv, &pv);

    printVariation(&child_pv);
    putchar('\n');
    fflush(stdout);

    char buff[6];
    PrintMoveStr(buff, m);
    printf("Best at depth %d: %s\n", depth, buff);

    pv.plies_count = child_pv.plies_count;
    memcpy(pv.plies, child_pv.plies, child_pv.plies_count * sizeof(Move));
  }

  return 0;
}
