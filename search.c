#include <stdio.h>

#include "main.h"

#define MAX_SCORE  2000000000
#define MATE_SCORE 1000000000

#define IS_WIN_MATE(s)  ((s) >= MATE_SCORE && (s) < MAX_SCORE)
#define IS_LOSE_MATE(s) ((s) <= -MATE_SCORE && (s) > -MAX_SCORE)

#define TT_SIZE (1024)

typedef struct
{
  Move plies[MAX_MOVES];
  int  plies_count;
} Variation;

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
  int           score;
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

int Evaluate(Board *b)
{
  int who2move[2] = {1, -1};

  int total = 0;

  for (int i = 0; i < 5; i++)
    total += (popcnt(b->piece[WHITE][i]) - popcnt(b->piece[BLACK][i])) * piece_value[i];

  return total * who2move[b->turn];
}

static void orderMoves(Move *moves, int moves_count, Move pv_move, int j)
{
  int best_order_score = -99999999;
  int best_i           = j;

  for (int i = j; i < moves_count; i++)
    if (j == 0 && pv_move != NULL_MOVE && moves[i] == pv_move)
    {
      best_i = i;
      break;
    }
    else
    {
      int order_score;
      switch (GET_TYPE(moves[i]))
      {
        case MOVE_TYPE_EP:
          order_score = GET_CAPTURED_PIECE(moves[i]) - 1;
          break;
        case MOVE_TYPE_CAPTURE:
          order_score = GET_CAPTURED_PIECE(moves[i]) - GET_PIECE(moves[i]);
          break;
        case MOVE_TYPE_PROMOTION:
          order_score = GET_PROMOTION_PIECE(moves[i]) - 1;
          break;
        case MOVE_TYPE_PROMOTION_WITH_CAPTURE:
          order_score = GET_PROMOTION_PIECE(moves[i]) + GET_CAPTURED_PIECE(moves[i]) -
                        GET_PIECE(moves[i]) - 1;
          break;
        default:
          order_score = -100;
          break;
      }
      if (order_score > best_order_score)
      {
        best_order_score = order_score;
        best_i           = i;
      }
    }

  Move tmp      = moves[j];
  moves[j]      = moves[best_i];
  moves[best_i] = tmp;
}
static int quiesce(Board *b, int alpha, int beta)
{
  int static_eval = Evaluate(b);

  if (static_eval > alpha) alpha = static_eval;
  if (alpha >= beta) return beta;

  Move moves[MAX_MOVES];
  int  moves_count;

  Generate(b, b->turn, GEN_ATTACKS, moves, &moves_count);

  for (int i = 0; i < moves_count; i++)
  {
    if (IsLegal(b, moves[i]))
    {
      MakeMove(b, moves[i]);
      int score = -quiesce(b, -beta, -alpha);
      UnmakeMove(b);

      if (score >= alpha) alpha = score;
      if (alpha >= beta) return beta;
    }
  }

  return alpha;
}

static int alphaBeta(Board *b, int alpha, int beta, int depthleft, Variation *variation,
                     Move *best_move, Variation *pv, Variation *previous_pv)
{
  int original_alpha = alpha;

  Variation child_pv;
  child_pv.plies_count = 0;

  if (depthleft == 0)
  {
    pv->plies_count = 0;
    return quiesce(b, alpha, beta);
  }

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
    Generate(b, b->turn, GEN_ALL, moves, &moves_count);
    pv_move = NULL_MOVE;
  }

  int legal_found = 0;

  for (int i = 0; i < moves_count; i++)
  {
    orderMoves(moves, moves_count, pv_move, i);

    if (IsLegal(b, moves[i]))
    {
      legal_found++;

      MakeMove(b, moves[i]);
      variation->plies[variation->plies_count] = moves[i];
      variation->plies_count++;

      int score =
          -alphaBeta(b, -beta, -alpha, depthleft - 1, variation, NULL, &child_pv, previous_pv);

      variation->plies_count--;
      UnmakeMove(b);

      if (score > alpha)
      {
        pv_move = moves[i];
        alpha   = score;

        pv->plies_count = child_pv.plies_count + 1;
        pv->plies[0]    = moves[i];
        for (int i = 0; i < child_pv.plies_count; i++) pv->plies[i + 1] = child_pv.plies[i];

        if (best_move != NULL)
        {
          *best_move = moves[i];
          pv_move    = moves[i];
        }
      }
      if (alpha >= beta) return beta;
    }
  }

  if (legal_found == 0)
  {
    if (IsKingAttacked(b, b->turn))
      alpha = -MATE_SCORE - variation->plies_count;
    else
      alpha = 0;

    pv->plies_count = 0;
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

  Variation previous_pv;
  previous_pv.plies_count = 0;

  for (int depth = 2; depth < 20; depth++)
  {
    Variation variation;
    variation.plies_count = 0;

    Variation pv;

    Move m     = 0;
    int  score = alphaBeta(b, -MAX_SCORE, MAX_SCORE, depth, &variation, &m, &pv, &previous_pv);

    char buff[6];
    PrintMoveStr(buff, m);
    printf("Best at depth %d: %s, (score: %d)\n", depth, buff, score);

    printf("PV: ");
    printVariation(&pv);
    putchar('\n');

    if (IS_LOSE_MATE(score))
    {
      printf("Mate in %d\n", -(pv.plies_count + 1) / 2);
      break;
    }
    if (IS_WIN_MATE(score))
    {
      printf("Mate in %d\n", (pv.plies_count + 1) / 2);
      break;
    }
  }

  return 0;
}
