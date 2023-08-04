#include "main.h"

#define NULL 0

static void genPawnAttacks(Board *b, Move *move_list, int *move_count, int side)
{
  int sq;
  BB  sq_bb;

  BB possible_attacks = b->pieces_of[!side];
  if (b->ep_possible) possible_attacks |= b->ep_square;

  BB attacks[2];  // Left and right captures

  if (side == WHITE)
  {
    attacks[0] = ((b->piece[side][PAWN] & ~FILE_A) << 7) & possible_attacks;
    attacks[1] = ((b->piece[side][PAWN] & ~FILE_H) << 9) & possible_attacks;
  }
  else
  {
    attacks[0] = ((b->piece[side][PAWN] & ~FILE_A) >> 9) & possible_attacks;
    attacks[1] = ((b->piece[side][PAWN] & ~FILE_H) >> 7) & possible_attacks;
  }

  for (int lr = 0; lr < 2; lr++)
  {
    while ((sq = ffsll(attacks[lr])) != 0)
    {
      sq--;
      sq_bb = SQ_TO_BB(sq);

      const int origin_direction[2][2] = {/* White: */ {-7, -9}, /* Black: */ {9, 7}};

      if (sq >= 56 || sq <= 7)  // Promotions
      {
        for (int i = 1; i < 5; i++)
        {
          move_list[*move_count] =
              CREATE_MOVE(sq + origin_direction[side][lr], sq, GetPieceAt(b, sq_bb), i, PAWN,
                          MOVE_TYPE_PROMOTION_WITH_CAPTURE);
          (*move_count)++;
        }
      }
      else  // No promotion
      {
        if (b->ep_possible && sq_bb == b->ep_square)  // EP
        {
          move_list[*move_count] =
              CREATE_MOVE(sq + origin_direction[side][lr], sq, PAWN, 0, PAWN, MOVE_TYPE_EP);
        }
        else  // Normal capture
        {
          move_list[*move_count] = CREATE_MOVE(sq + origin_direction[side][lr], sq,
                                               GetPieceAt(b, sq_bb), 0, PAWN, MOVE_TYPE_CAPTURE);
        }
        (*move_count)++;
      }

      attacks[lr] &= ~sq_bb;
    }
  };
}

static void genPawnPushes(Board *b, Move *move_list, int *move_count, int side)
{
  int sq;
  BB  sq_bb;

  BB pushes[2];

  if (side == WHITE)
  {
    pushes[0] = (b->piece[side][PAWN] << 8) & ~RANK_8 & ~b->all_pieces;
    pushes[1] = ((pushes[0] & 0xff0000) << 8) & ~b->all_pieces;
  }
  else
  {
    pushes[0] = (b->piece[side][PAWN] >> 8) & ~RANK_1 & ~b->all_pieces;
    pushes[1] = ((pushes[0] & 0xff0000000000) >> 8) & ~b->all_pieces;
  }

  while ((sq = ffsll(pushes[0])) != 0)
  {
    const int origin_direction[2] = {-8, 8};

    sq--;
    sq_bb = SQ_TO_BB(sq);

    move_list[*move_count] =
        CREATE_MOVE(sq + origin_direction[side], sq, 0, 0, PAWN, MOVE_TYPE_SILENT);
    (*move_count)++;

    pushes[0] &= ~sq_bb;
  }

  while ((sq = ffsll(pushes[1])) != 0)
  {
    const int origin_direction[2] = {-16, 16};

    sq--;
    sq_bb = SQ_TO_BB(sq);

    move_list[*move_count] =
        CREATE_MOVE(sq + origin_direction[side], sq, 0, 0, PAWN, MOVE_TYPE_DOUBLE_PUSH);
    (*move_count)++;

    pushes[1] &= ~sq_bb;
  }
}

static void genPromotions(Board *b, Move *move_list, int *move_count, int side)
{
  int sq;
  BB  sq_bb;

  BB promotions;

  if (side == WHITE)
    promotions = (b->piece[side][PAWN] << 8) & RANK_8 & ~b->all_pieces;
  else
    promotions = (b->piece[side][PAWN] >> 8) & RANK_1 & ~b->all_pieces;

  while ((sq = ffsll(promotions)) != 0)
  {
    const int origin_direction[2] = {-8, 8};

    sq--;
    sq_bb = SQ_TO_BB(sq);

    for (int i = 1; i < 5; i++)
    {
      move_list[*move_count] =
          CREATE_MOVE(sq + origin_direction[side], sq, 0, i, PAWN, MOVE_TYPE_PROMOTION);

      (*move_count)++;
    }

    promotions &= ~sq_bb;
  }
}

static void genKnight(Board *b, Move *move_list, int *move_count, int side, int type)
{
  int knight, sq;
  BB  sq_bb;

  BB knights = b->piece[side][KNIGHT];

  while ((knight = ffsll(knights)) != 0)
  {
    knight--;

    BB destinations = precomp_knight_moves[knight];

    switch (type)
    {
      case GEN_SILENT:
        destinations &= ~b->all_pieces;
        break;
      case GEN_ATTACKS:
        destinations &= b->pieces_of[!side];
        break;
      case GEN_ALL:
        destinations &= ~b->pieces_of[side];
        break;
    }

    while ((sq = ffsll(destinations)) != 0)
    {
      sq--;
      sq_bb = SQ_TO_BB(sq);

      if ((sq_bb & b->pieces_of[!side]) != 0)
      {
        move_list[*move_count] =
            CREATE_MOVE(knight, sq, GetPieceAt(b, sq_bb), 0, KNIGHT, MOVE_TYPE_CAPTURE);
      }
      else
      {
        move_list[*move_count] = CREATE_MOVE(knight, sq, 0, 0, KNIGHT, MOVE_TYPE_SILENT);
      }
      (*move_count)++;

      destinations &= ~sq_bb;
    }

    knights &= ~SQ_TO_BB(knight);
  }
}

static void genKing(Board *b, Move *move_list, int *move_count, int side, int type)
{
  int sq, king;
  BB  sq_bb;

  king = BB_TO_SQ(b->piece[side][KING]);

  BB destinations = precomp_king_moves[king];

  switch (type)
  {
    case GEN_SILENT:
      destinations &= ~b->all_pieces;
      break;
    case GEN_ATTACKS:
      destinations &= b->pieces_of[!side];
      break;
    case GEN_ALL:
      destinations &= ~b->pieces_of[side];
      break;
  }

  while ((sq = ffsll(destinations)) != 0)
  {
    sq--;
    sq_bb = SQ_TO_BB(sq);

    if ((sq_bb & b->pieces_of[!side]) != 0)
    {
      move_list[*move_count] =
          CREATE_MOVE(king, sq, GetPieceAt(b, sq_bb), 0, KING, MOVE_TYPE_CAPTURE);
    }
    else
    {
      move_list[*move_count] = CREATE_MOVE(king, sq, 0, 0, KING, MOVE_TYPE_SILENT);
    }
    (*move_count)++;

    destinations &= ~sq_bb;
  }
}

static void genSliding(Board *b, Move *move_list, int *move_count, int side, int type)
{
  int sq, piece;
  BB  sq_bb;

  for (int p = ROOK; p <= BISHOP; p++)
  {
    BB pieces = b->piece[side][p];

    while ((piece = ffsll(pieces)) != 0)
    {
      piece--;
      BB piece_bb = SQ_TO_BB(piece);

      BB destinations = 0;

      if (p == ROOK | p == QUEEN)
      {
        BB blockers = b->all_pieces;
        blockers &= precomp_rook_blocker_mask[piece];
        destinations |=
            precomp_rook_moves[piece][(blockers * precomp_rook_magic[piece]) >> (64 - 12)];
      }
      if (p == BISHOP | p == QUEEN)
      {
        BB blockers = b->all_pieces;
        blockers &= precomp_bishop_blocker_mask[piece];
        destinations |=
            precomp_bishop_moves[piece][(blockers * precomp_bishop_magic[piece]) >> (64 - 9)];
      }

      switch (type)
      {
        case GEN_SILENT:
          destinations &= ~b->all_pieces;
          break;
        case GEN_ATTACKS:
          destinations &= b->pieces_of[!side];
          break;
        case GEN_ALL:
          destinations &= ~b->pieces_of[side];
          break;
      }

      while ((sq = ffsll(destinations)) != 0)
      {
        sq--;
        sq_bb = SQ_TO_BB(sq);

        if ((sq_bb & b->pieces_of[!side]) != 0)
        {
          move_list[*move_count] =
              CREATE_MOVE(piece, sq, GetPieceAt(b, sq_bb), 0, p, MOVE_TYPE_CAPTURE);
        }
        else
        {
          move_list[*move_count] = CREATE_MOVE(piece, sq, 0, 0, p, MOVE_TYPE_SILENT);
        }
        (*move_count)++;

        destinations &= ~sq_bb;
      }

      pieces &= ~piece_bb;
    }
  }
}

static void genCastles(Board *b, Move *move_list, int *move_count, int side)
{
  const BB pass_through_k[2] = {0x60, 0x6000000000000000};
  const BB pass_through_q[2] = {0xe, 0xe00000000000000};

  if ((b->castle[side] & CASTLE_K) == CASTLE_K && (b->all_pieces & pass_through_k[side]) == 0)
  {
    const int pass_through_sq[3][3] = {{4, 5, 6}, {60, 61, 62}};

    int not_attacked = 1;
    for (int i = 0; i < 3; i++)
      if (SquareAttackedBy(b, !side, pass_through_sq[side][i]))
      {
        not_attacked = 0;
        break;
      }

    if (not_attacked)
    {
      move_list[*move_count] = CREATE_MOVE(0, 0, 0, 0, KING, MOVE_TYPE_CASTLE_K);
      (*move_count)++;
    }
  }
  if ((b->castle[side] & CASTLE_Q) == CASTLE_Q && (b->all_pieces & pass_through_q[side]) == 0)
  {
    const int pass_through_sq[3][3] = {{4, 3, 2}, {60, 59, 58}};

    int not_attacked = 1;
    for (int i = 0; i < 3; i++)
      if (SquareAttackedBy(b, !side, pass_through_sq[side][i]))
      {
        not_attacked = 0;
        break;
      }

    if (not_attacked)
    {
      move_list[*move_count] = CREATE_MOVE(0, 0, 0, 0, KING, MOVE_TYPE_CASTLE_Q);
      (*move_count)++;
    }
  }
}

void Generate(Board *b, int side, int type, Move move_list[], int *move_count)
{
  *move_count = 0;

  if ((type & GEN_ATTACKS) == GEN_ATTACKS)
  {
    genPawnAttacks(b, move_list, move_count, side);
    genPromotions(b, move_list, move_count, side);
    genKnight(b, move_list, move_count, side, GEN_ATTACKS);
    genSliding(b, move_list, move_count, side, GEN_ATTACKS);
    genKing(b, move_list, move_count, side, GEN_ATTACKS);
  }
  if ((type & GEN_SILENT) == GEN_SILENT)
  {
    genPawnPushes(b, move_list, move_count, side);
    genKnight(b, move_list, move_count, side, GEN_SILENT);
    genSliding(b, move_list, move_count, side, GEN_SILENT);
    genKing(b, move_list, move_count, side, GEN_SILENT);
    genCastles(b, move_list, move_count, side);
  }
}
