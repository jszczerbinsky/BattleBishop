#include <stdio.h>
#include <string.h>

#include "main.h"

static const int piece_value[6] = {
    100,  // Pawn
    300,  // Knight
    500,  // Rook
    900,  // Queen
    300,  // Bishop
};

int PieceToHashIndex(int piece, int player)
{
  if (player == BLACK)
    return piece + 6;
  else
    return piece;
}

static int popcnt(BB b)
{
  int sq;
  int i = 0;
  while ((sq = ffsll((long long)b)) != 0)
  {
    i++;
    b &= ~SQ_TO_BB(sq - 1);
  };
  return i;
}

int GetTotalMaterial(Board *b, int side)
{
  int total = 0;

  for (int i = 0; i < 5; i++) total += popcnt(b->piece[side][i]) * piece_value[i];

  return total;
}

int IsEndgmae(Board *b)
{
  int total_b = 0;
  int total_w = 0;

  // Max 3 pieces other than pawns and kings
  for (int i = 1; i < 5; i++)
  {
    total_w += popcnt(b->piece[WHITE][i]);
    total_b += popcnt(b->piece[BLACK][i]);
  }

  return total_w <= 3 && total_b <= 3;
}

int SquareAttackedBy(Board *b, int side, int sq)
{
  BB sq_bb = SQ_TO_BB(sq);

  if (side == WHITE)
  {
    if (((((sq_bb & ~FILE_H) >> 7) | ((sq_bb & ~FILE_A) >> 9)) & b->piece[side][PAWN]) != 0)
      return 1;
  }
  else
  {
    if (((((sq_bb & ~FILE_H) << 9) | ((sq_bb & ~FILE_A) << 7)) & b->piece[side][PAWN]) != 0)
      return 1;
  }
  if ((precomp_knight_moves[sq] & b->piece[side][KNIGHT]) != 0) return 1;
  if ((precomp_king_moves[sq] & b->piece[side][KING]) != 0) return 1;

  BB rook_blocker = precomp_rook_blocker_mask[sq] & b->all_pieces;
  if ((precomp_rook_moves[sq][(rook_blocker * precomp_rook_magic[sq]) >> (64 - 12)] &
       (b->piece[side][ROOK] | b->piece[side][QUEEN])) != 0)
    return 1;

  BB bishop_blocker = precomp_bishop_blocker_mask[sq] & b->all_pieces;
  if ((precomp_bishop_moves[sq][(bishop_blocker * precomp_bishop_magic[sq]) >> (64 - 9)] &
       (b->piece[side][QUEEN] | b->piece[side][BISHOP])) != 0)
    return 1;

  return 0;
}

int IsKingAttacked(Board *b, int side)
{
  return SquareAttackedBy(b, !side, ffsll((long long)b->piece[side][KING]) - 1);
}

int IsLegal(Board *b, Move m)
{
  MakeMove(b, m);
  int legal = !IsKingAttacked(b, !b->turn);
  UnmakeMove(b);

  return legal;
}

void updateBitboards(Board *b)
{
  b->pieces_of[WHITE] = 0;
  b->pieces_of[BLACK] = 0;

  for (int i = 0; i < 2; i++)
    for (int j = 0; j < 6; j++) b->pieces_of[i] |= b->piece[i][j];

  b->all_pieces = b->pieces_of[WHITE] | b->pieces_of[BLACK];

  b->hash_value = 0;
  for (int i = 0; i < 2; i++)
  {
    for (int j = 0; j < 64; j++)
    {
      BB sq_bb = SQ_TO_BB(j);
      if ((sq_bb & b->pieces_of[i]) != 0)
        b->hash_value ^= precomp_hash[j][PieceToHashIndex(GetPieceAt(b, sq_bb), i)];
    }

    if (b->castle[i] & CASTLE_K) b->hash_value ^= precomp_hash_castle[i][CASTLE_K - 1];
    if (b->castle[i] & CASTLE_Q) b->hash_value ^= precomp_hash_castle[i][CASTLE_Q - 1];
  }

  b->hash_value ^= precomp_hash_turn[b->turn];

  if (b->ep_possible) b->hash_value ^= precomp_hash_ep[ffsll((long long)b->ep_square) - 1];
}

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

  b->castle[WHITE] = CASTLE_K | CASTLE_Q;
  b->castle[BLACK] = CASTLE_K | CASTLE_Q;

  b->halfmove = 0;
  b->fullmove = 0;

  b->ep_possible = 0;

  b->variation.plies_count = 0;

  updateBitboards(b);
}

void FEN(Board *b, char *str)
{
  memset(b, 0, sizeof(Board));

  int y = 7;
  int x = 0;
  while (*str != ' ')
  {
    int i = y * 8 + x;

    if (*str >= '0' && *str <= '8')
      x += *str - '0';
    else if (*str == '/')
    {
      y--;
      x = 0;
    }
    else
    {
      switch (*str)
      {
        case 'P':
          b->piece[WHITE][PAWN] |= SQ_TO_BB(i);
          break;
        case 'R':
          b->piece[WHITE][ROOK] |= SQ_TO_BB(i);
          break;
        case 'B':
          b->piece[WHITE][BISHOP] |= SQ_TO_BB(i);
          break;
        case 'Q':
          b->piece[WHITE][QUEEN] |= SQ_TO_BB(i);
          break;
        case 'K':
          b->piece[WHITE][KING] |= SQ_TO_BB(i);
          break;
        case 'N':
          b->piece[WHITE][KNIGHT] |= SQ_TO_BB(i);
          break;
        case 'p':
          b->piece[BLACK][PAWN] |= SQ_TO_BB(i);
          break;
        case 'r':
          b->piece[BLACK][ROOK] |= SQ_TO_BB(i);
          break;
        case 'b':
          b->piece[BLACK][BISHOP] |= SQ_TO_BB(i);
          break;
        case 'q':
          b->piece[BLACK][QUEEN] |= SQ_TO_BB(i);
          break;
        case 'k':
          b->piece[BLACK][KING] |= SQ_TO_BB(i);
          break;
        case 'n':
          b->piece[BLACK][KNIGHT] |= SQ_TO_BB(i);
          break;
      }
      x++;
    }
    str++;
  }

  for (int i = 0; i < 2; i++)
    for (int j = 0; j < 6; j++) b->pieces_of[i] |= b->piece[i][j];

  b->all_pieces = b->pieces_of[WHITE] | b->pieces_of[BLACK];

  str++;
  if (*str == 'b') b->turn = BLACK;
  str += 2;

  while (*str != ' ')
  {
    switch (*str)
    {
      default:
        break;
      case 'K':
        b->castle[WHITE] |= CASTLE_K;
        break;
      case 'Q':
        b->castle[WHITE] |= CASTLE_Q;
        break;
      case 'k':
        b->castle[BLACK] |= CASTLE_K;
        break;
      case 'q':
        b->castle[BLACK] |= CASTLE_Q;
        break;
    }
    str++;
  }

  str++;

  if (*str != '-')
  {
    b->ep_possible = 1;
    b->ep_square   = SQ_TO_BB((*str - 'a') + 8 * (*(str + 1) - '1'));
    str++;
  }
  str += 2;

  updateBitboards(b);
}

int GetPieceAt(Board *b, BB s)
{
  for (int i = 0; i < 6; i++)
    for (int j = 0; j < 2; j++)
      if ((s & b->piece[j][i]) == s) return i;

  printf("GetPieceAt - empty square\n");
  return 0;
}

void PrintMoveStr(char buff[], Move m)
{
  if (m == NULL_MOVE)
    sprintf(buff, "null");
  else if (GET_TYPE(m) == MOVE_TYPE_CASTLE_K)
    sprintf(buff, "O-O");
  else if (GET_TYPE(m) == MOVE_TYPE_CASTLE_Q)
    sprintf(buff, "O-O-O");
  else
  {
    int origin = GET_ORIGIN_SQ(m);
    int dest   = GET_DEST_SQ(m);

    const char piece_char[] = {
        'p',
        'n',
        'r',
        'q',
        'b',
        'k',
    };

    sprintf(
        buff,
        "%c%c%c%c%c",
        (origin % 8) + 'a',
        (origin / 8) + '1',
        (dest % 8) + 'a',
        (dest / 8) + '1',
        (GET_TYPE(m) == MOVE_TYPE_PROMOTION) || (GET_TYPE(m) == MOVE_TYPE_PROMOTION_WITH_CAPTURE)
            ? piece_char[GET_PROMOTION_PIECE(m)]
            : '\0'
    );
  }
}

void PrintBoard(Board *b)
{
  const char piece_char[2][6] = {
      {'P', 'N', 'R', 'Q', 'B', 'K'},
      {'p', 'n', 'r', 'q', 'b', 'k'},
  };

  for (int y = 7; y >= 0; y--)
  {
    for (int x = 0; x < 8; x++)
    {
      int sq    = x + y * 8;
      BB  sq_bb = SQ_TO_BB(sq);

      if ((sq_bb & b->pieces_of[WHITE]) != 0)
        putchar(piece_char[WHITE][GetPieceAt(b, sq_bb)]);
      else if ((sq_bb & b->pieces_of[BLACK]) != 0)
        putchar(piece_char[BLACK][GetPieceAt(b, sq_bb)]);
      else
        putchar('.');

      putchar(' ');
    }
    printf("\n");
  }
  if (b->ep_possible)
    printf(
        "Ep square: %c%c\n",
        (ffsll((long long)b->ep_square) - 1) % 8 + 'a',
        (ffsll((long long)b->ep_square) - 1) / 8 + '1'
    );
}

static void updateCastlingAfterCapture(Board *b, BB dest_bb)
{
  switch (dest_bb)
  {
    case 0x1:
      b->castle[WHITE] &= ~CASTLE_Q;
      break;
    case 0x80:
      b->castle[WHITE] &= ~CASTLE_K;
      break;
    case 0x100000000000000:
      b->castle[BLACK] &= ~CASTLE_Q;
      break;
    case 0x8000000000000000:
      b->castle[BLACK] &= ~CASTLE_K;
      break;
    default:
      break;
  }
}

void MakeMove(Board *b, Move m)
{
  b->variation.plies[b->variation.plies_count]            = m;
  b->state_backup[b->variation.plies_count].ep_possible   = b->ep_possible;
  b->state_backup[b->variation.plies_count].ep_square     = b->ep_square;
  b->state_backup[b->variation.plies_count].castle[WHITE] = b->castle[WHITE];
  b->state_backup[b->variation.plies_count].castle[BLACK] = b->castle[BLACK];
  b->state_backup[b->variation.plies_count].halfmove      = b->halfmove;
  b->state_backup[b->variation.plies_count].hash_value    = b->hash_value;

  b->variation.plies_count++;

  if (m != NULL_MOVE)
  {
    b->ep_possible = 0;

    int piece = GET_PIECE(m);

    BB origin_bb = SQ_TO_BB(GET_ORIGIN_SQ(m));
    BB dest_bb   = SQ_TO_BB(GET_DEST_SQ(m));

    if (piece == KING)
      b->castle[b->turn] = 0;
    else if (piece == ROOK)
    {
      switch (origin_bb)
      {
        case 0x10:
          b->castle[WHITE] = 0;
          break;
        case 0x1000000000000000:
          b->castle[BLACK] = 0;
          break;
        case 0x1:
          b->castle[WHITE] &= ~CASTLE_Q;
          break;
        case 0x80:
          b->castle[WHITE] &= ~CASTLE_K;
          break;
        case 0x100000000000000:
          b->castle[BLACK] &= ~CASTLE_Q;
          break;
        case 0x8000000000000000:
          b->castle[BLACK] &= ~CASTLE_K;
          break;
        default:
          break;
      }
    }

    switch (GET_TYPE(m))
    {
      case MOVE_TYPE_SILENT:
        b->piece[b->turn][piece] ^= (origin_bb | dest_bb);
        break;
      case MOVE_TYPE_DOUBLE_PUSH:
        b->piece[b->turn][piece] ^= (origin_bb | dest_bb);
        b->ep_possible = 1;
        b->ep_square   = b->turn == WHITE ? (dest_bb >> 8) : (dest_bb << 8);
        break;
      case MOVE_TYPE_EP:
        b->piece[b->turn][piece] ^= (origin_bb | dest_bb);
        b->piece[!b->turn][PAWN] &= ~(b->turn == WHITE ? (b->ep_square >> 8) : (b->ep_square << 8));
        break;
      case MOVE_TYPE_CAPTURE:
        b->piece[b->turn][piece] ^= (origin_bb | dest_bb);
        b->piece[!b->turn][GET_CAPTURED_PIECE(m)] &= ~dest_bb;
        updateCastlingAfterCapture(b, dest_bb);
        break;
      case MOVE_TYPE_PROMOTION:
        b->piece[b->turn][PAWN] &= ~origin_bb;
        b->piece[b->turn][GET_PROMOTION_PIECE(m)] |= dest_bb;
        break;
      case MOVE_TYPE_PROMOTION_WITH_CAPTURE:
        b->piece[b->turn][PAWN] &= ~origin_bb;
        b->piece[b->turn][GET_PROMOTION_PIECE(m)] |= dest_bb;
        b->piece[!b->turn][GET_CAPTURED_PIECE(m)] &= ~dest_bb;
        updateCastlingAfterCapture(b, dest_bb);
        break;
      case MOVE_TYPE_CASTLE_K:
        if (b->turn == WHITE)
        {
          b->piece[b->turn][KING] = 0x40;
          b->piece[b->turn][ROOK] ^= 0xa0;
          b->castle[WHITE] = 0;
        }
        else
        {
          b->piece[b->turn][KING] = 0x4000000000000000;
          b->piece[b->turn][ROOK] ^= 0xa000000000000000;
          b->castle[BLACK] = 0;
        }
        break;
      case MOVE_TYPE_CASTLE_Q:
        if (b->turn == WHITE)
        {
          b->piece[b->turn][KING] = 0x4;
          b->piece[b->turn][ROOK] ^= 0x9;
          b->castle[WHITE] = 0;
        }
        else
        {
          b->piece[b->turn][KING] = 0x400000000000000;
          b->piece[b->turn][ROOK] ^= 0x900000000000000;
          b->castle[BLACK] = 0;
        }
        break;
    }
  }
  else
    b->ep_possible = 0;

  b->turn = !b->turn;

  updateBitboards(b);
}

void UnmakeMove(Board *b)
{
  b->turn = !b->turn;

  b->variation.plies_count--;

  Move m = b->variation.plies[b->variation.plies_count];

  b->ep_possible   = b->state_backup[b->variation.plies_count].ep_possible;
  b->ep_square     = b->state_backup[b->variation.plies_count].ep_square;
  b->castle[WHITE] = b->state_backup[b->variation.plies_count].castle[WHITE];
  b->castle[BLACK] = b->state_backup[b->variation.plies_count].castle[BLACK];
  b->halfmove      = b->state_backup[b->variation.plies_count].halfmove;
  b->hash_value    = b->state_backup[b->variation.plies_count].hash_value;

  if (m != NULL_MOVE)
  {
    int piece = GET_PIECE(m);

    BB origin_bb = SQ_TO_BB(GET_ORIGIN_SQ(m));
    BB dest_bb   = SQ_TO_BB(GET_DEST_SQ(m));

    switch (GET_TYPE(m))
    {
      case MOVE_TYPE_SILENT:
        b->piece[b->turn][piece] ^= (origin_bb | dest_bb);
        break;
      case MOVE_TYPE_DOUBLE_PUSH:
        b->piece[b->turn][piece] ^= (origin_bb | dest_bb);
        break;
      case MOVE_TYPE_EP:
        b->piece[b->turn][piece] ^= (origin_bb | dest_bb);
        b->piece[!b->turn][PAWN] |= (b->turn == WHITE ? (b->ep_square >> 8) : (b->ep_square << 8));
        break;
      case MOVE_TYPE_CAPTURE:
        b->piece[b->turn][piece] ^= (origin_bb | dest_bb);
        b->piece[!b->turn][GET_CAPTURED_PIECE(m)] |= dest_bb;
        break;
      case MOVE_TYPE_PROMOTION:
        b->piece[b->turn][PAWN] |= origin_bb;
        b->piece[b->turn][GET_PROMOTION_PIECE(m)] &= ~dest_bb;
        break;
      case MOVE_TYPE_PROMOTION_WITH_CAPTURE:
        b->piece[b->turn][PAWN] |= origin_bb;
        b->piece[b->turn][GET_PROMOTION_PIECE(m)] &= ~dest_bb;
        b->piece[!b->turn][GET_CAPTURED_PIECE(m)] |= dest_bb;
        break;
      case MOVE_TYPE_CASTLE_K:
        if (b->turn == WHITE)
        {
          b->piece[b->turn][KING] = 0x10;
          b->piece[b->turn][ROOK] ^= 0xa0;
        }
        else
        {
          b->piece[b->turn][KING] = 0x1000000000000000;
          b->piece[b->turn][ROOK] ^= 0xa000000000000000;
        }
        break;
      case MOVE_TYPE_CASTLE_Q:
        if (b->turn == WHITE)
        {
          b->piece[b->turn][KING] = 0x10;
          b->piece[b->turn][ROOK] ^= 0x9;
        }
        else
        {
          b->piece[b->turn][KING] = 0x1000000000000000;
          b->piece[b->turn][ROOK] ^= 0x900000000000000;
        }
        break;
    }
  }

  updateBitboards(b);
}
