#include <stdio.h>

#include "main.h"

#define MAX_SCORE      2000000000
#define MATE_SCORE     1000000000
#define MIN_MATE_SCORE 500000000

#define IS_WIN_MATE(s)  ((s) >= MIN_MATE_SCORE && (s) <= MATE_SCORE)
#define IS_LOSE_MATE(s) ((s) <= -MIN_MATE_SCORE && (s) >= -MATE_SCORE)

#define TT_SIZE (1024)

#define TTENTRY_EXACT      0
#define TTENTRY_LOWERBOUND 1
#define TTENTRY_UPPERBOUND 2

typedef struct
{
  BB            hash;
  Move          best_move;
  Move          moves[MAX_MOVES];
  int           moves_count;
  int           score;
  unsigned char occupied;
  unsigned char depth;
  unsigned char entry_type;
} TTEntry;

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

int Evaluate(Board *b)
{
  int who2move[2] = {1, -1};

  int total = 0;

  for (int i = 0; i < 5; i++) total += GetTotalMaterial(b, WHITE) - GetTotalMaterial(b, BLACK);

  return total * who2move[b->turn];
}

static void orderMoves(Move *moves, int moves_count, Move pv_move, Move best_move, int j)
{
  const int pv_move_score   = 9999999;
  const int best_move_score = 8888888;

  int best_order_score = -pv_move_score;
  int best_i           = j;

  for (int i = j; i < moves_count; i++)
  {
    int order_score;

    if (moves[i] == pv_move)
      order_score = pv_move_score;
    else if (moves[i] == best_move)
      order_score = best_move_score;
    else
    {
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

static int isPv(Variation *pv, Variation *variation)
{
  if (variation->plies_count > pv->plies_count + 1) return 0;

  for (int i = 0; i < variation->plies_count; i++)
    if (variation->plies[i] != pv->plies[i]) return 0;
  return 1;
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
    orderMoves(moves, moves_count, NULL_MOVE, NULL_MOVE, i);

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

static int alphaBeta(Board *b, int alpha, int beta, int depthleft, Variation *pv,
                     Variation *previous_pv, int can_null)
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

  Move best_move;

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
    best_move = tt->best_move;
  }
  else
  {
    Generate(b, b->turn, GEN_ALL, moves, &moves_count);
    best_move = NULL_MOVE;
  }

  int in_check = IsKingAttacked(b, b->turn);

  // Null move pruning
  if (can_null && IsEndgmae(b)) can_null = 0;
  if (can_null && depthleft >= 4 && !in_check)
  {
    MakeMove(b, NULL_MOVE);

    int null_move_score = -alphaBeta(b, -beta, 1 - beta, depthleft - 4, &child_pv, previous_pv, 0);

    UnmakeMove(b);

    if (null_move_score >= beta) return beta;
  }

  int legal_found = 0;

  int any_child_failed_high = 0;
  int reduced               = 0;

  // Check extension
  if (in_check) depthleft++;

  for (int i = 0; i < moves_count; i++)
  {
    Move pv_move = NULL_MOVE;
    if (isPv(previous_pv, &b->variation)) pv_move = previous_pv->plies[b->variation.plies_count];
    orderMoves(moves, moves_count, pv_move, best_move, i);

    if (IsLegal(b, moves[i]))
    {
      legal_found++;

      MakeMove(b, moves[i]);

      // Late move reduction
      if (depthleft >= 3 && GET_TYPE(moves[i]) == MOVE_TYPE_SILENT && !reduced &&
          !any_child_failed_high && legal_found > 4)
      {
        reduced = 1;
        depthleft--;
      }

      int score = -alphaBeta(b, -beta, -alpha, depthleft - 1, &child_pv, previous_pv, can_null);

      if (score == beta) any_child_failed_high = 1;

      UnmakeMove(b);

      if (score > alpha)
      {
        best_move = moves[i];
        alpha     = score;

        pv->plies_count = child_pv.plies_count + 1;
        pv->plies[0]    = moves[i];
        for (int i = 0; i < child_pv.plies_count; i++) pv->plies[i + 1] = child_pv.plies[i];
      }
      if (alpha >= beta) return beta;
    }
  }

  if (legal_found == 0)
  {
    if (IsKingAttacked(b, b->turn))
      alpha = -MATE_SCORE + b->variation.plies_count;
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
  ttEntry->best_move = best_move;
  ttEntry->occupied  = 1;

  return alpha;
}

Move Search(Board *b)
{
  memset(tt, 0, TT_SIZE * sizeof(TTEntry));

  Variation previous_pv;
  previous_pv.plies_count = 0;

  for (int depth = 2; depth < 20; depth++)
  {
    Variation pv;

    Move m     = 0;
    int  score = alphaBeta(b, -MAX_SCORE, MAX_SCORE, depth, &pv, &previous_pv, 1);

    m = pv.plies[0];

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
