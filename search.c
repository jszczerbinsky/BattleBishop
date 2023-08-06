#include <math.h>
#include <stdio.h>

#include "main.h"

#define TT_SIZE (1024 * 1024)

typedef struct
{
  Move plies[MAX_MOVES];
  int  plies_count;
} Variation;

#define MATE_SCORE     9999999999.9
#define MATE_SCORE_MIN 99999999.9

#define TTENTRY_EXACT      0
#define TTENTRY_LOWERBOUND 1
#define TTENTRY_UPPERBOUND 2

typedef struct
{
  unsigned char occupied;
  BB            hash;
  unsigned char depth;
  Move          pv_move;
  Move          moves[MAX_MOVES];
  int           moves_count;
  float         score;
  unsigned char entry_type;
} TTEntry;

static const int piece_value[6] = {
    100,  // Pawn
    300,  // Knight
    500,  // Rook
    900,  // Queen
    300,  // Bishop
};

TTEntry tt[TT_SIZE];

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

static void orderMoves(Move *moves, int moves_count, Move pv_move)
{
  for (int i = 0; i < moves_count; i++)
    if (moves[i] == pv_move)
    {
      Move tmp = moves[0];
      moves[0] = moves[i];
      moves[i] = tmp;
    }
}

static float alphaBeta(Board *b, float alpha, float beta, int depthleft, Variation *variation,
                       Move *best_move)
{
  float original_alpha = alpha;

  if (depthleft == 0)
  {
    float score = Evaluate(b);
    return score;
  }

  // Only silents moves on leaves
  int move_type = /*depthleft == 1 ? GEN_SILENT :*/ GEN_ALL;

  Move moves[MAX_MOVES];
  int  moves_count;

  Move pv_move;

  // Read from tt
  TTEntry *ttEntry = tt + (b->hash_value % TT_SIZE);

  if (ttEntry->occupied == 1 && ttEntry->hash == b->hash_value)
  {
    if (ttEntry->depth >= depthleft)
    {
      switch (ttEntry->entry_type)
      {
        case TTENTRY_EXACT:
          return ttEntry->score;
          break;
        case TTENTRY_LOWERBOUND:
          if (ttEntry->score < beta) beta = ttEntry->score;
          break;
        case TTENTRY_UPPERBOUND:
          if (ttEntry->score > alpha) alpha = ttEntry->score;
          break;
      }

      if (alpha > beta) return ttEntry->score;
    }
    moves_count = ttEntry->moves_count;
    for (int i = 0; i < moves_count; i++) moves[i] = ttEntry->moves[i];
    pv_move = tt->pv_move;
  }
  else
  {
    Generate(b, b->turn, move_type, moves, &moves_count);
    pv_move = moves[0];
  }

  int legal_found = 0;

  orderMoves(moves, moves_count, pv_move);

  for (int i = 0; i < moves_count; i++)
  {
    if (IsLegal(b, moves[i]))
    {
      legal_found++;

      MakeMove(b, moves[i]);
      variation->plies[variation->plies_count] = moves[i];
      variation->plies_count++;

      float score = -alphaBeta(b, -beta, -alpha, depthleft - 1, variation, NULL);

      variation->plies_count--;
      UnmakeMove(b);

      if (score >= beta) return score;
      if (score > alpha)
      {
        pv_move = moves[i];
        alpha   = score;

        if (best_move != NULL)
        {
          *best_move = moves[i];
          pv_move    = moves[i];
        }
      }
    }
  }

  if (legal_found == 0)
  {
    if (IsKingAttacked(b, b->turn))
      alpha = -MATE_SCORE;
    else
      alpha = 0;
  }

  // Save to tt
  ttEntry->occupied    = 0;
  ttEntry->depth       = depthleft;
  ttEntry->hash        = b->hash_value;
  ttEntry->moves_count = moves_count;
  ttEntry->score       = alpha;
  if (alpha <= original_alpha)
    ttEntry->entry_type = TTENTRY_UPPERBOUND;
  else if (alpha >= beta)
    ttEntry->entry_type = TTENTRY_LOWERBOUND;
  else
    ttEntry->entry_type = TTENTRY_EXACT;
  for (int i = 0; i < moves_count; i++) ttEntry->moves[i] = moves[i];
  ttEntry->pv_move  = pv_move;
  ttEntry->occupied = 1;

  return alpha;
}

Move Search(Board *b)
{
  memset(tt, 0, TT_SIZE * sizeof(TTEntry));

  for (int depth = 2; depth < 20; depth++)
  {
    Variation variation;
    variation.plies_count = 0;

    Move  m     = 0;
    float score = alphaBeta(b, -INFINITY, INFINITY, depth, &variation, &m);

    char buff[6];
    PrintMoveStr(buff, m);
    printf("Best at depth %d: %s, (score: %f)\n", depth, buff, score);

    if (score >= MATE_SCORE_MIN)
    {
      printf("Mate\n");
      break;
    }
  }

  return 0;
}
