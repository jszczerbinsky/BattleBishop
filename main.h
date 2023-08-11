#ifndef MAIN_H
#define MAIN_H

//
// Sides
//
#define WHITE 0
#define BLACK 1

//
// Piece types
//

#define PAWN   0
#define KNIGHT 1
#define ROOK   2
#define QUEEN  3
#define BISHOP 4
#define KING   5

#define PIECE_NONE 6

#define CASTLE_K 1
#define CASTLE_Q 2

//
// Ranks, files, diags
//
#define FILE_A 0x101010101010101
#define FILE_H 0x8080808080808080
#define RANK_8 0xff00000000000000
#define RANK_1 0xff
#define DIAG   0x8040201008040201

//
// Bitboards and squares
//

#define SQ_TO_BB(sq) (1LL << (sq))
#define BB_TO_SQ(sq) (ffsll(sq) - 1)

#define FLIP_SQ(sq) ((sq) ^ 56)

#define BASE_FILE(bb, f) ((bb) >> (f))
#define BASE_RANK(bb, r) ((bb) >> ((r)*8))
#define FILE_TO_RANK(bb) (((bb)*DIAG) >> 56)
#define RANK_TO_FILE(bb) ((((bb)*DIAG) >> 7) & FILE_A)
#define DIAG_TO_RANK(bb) (((bb)*FILE_A) >> 56)

//
// Moves
//

#define GEN_SILENT     1
#define GEN_ATTACKS    2
#define GEN_PROMOTIONS 4
#define GEN_ALL        (GEN_SILENT | GEN_ATTACKS | GEN_PROMOTIONS)

#define MAX_MOVES 256

#define NULL_MOVE 0b00000000000000000000000000000000

#define MOVE_ORIGIN_MASK          0b00000000000000000000000000111111
#define MOVE_DEST_MASK            0b00000000000000000000111111000000
#define MOVE_CAPTURED_PIECE_MASK  0b00000000000000000111000000000000
#define MOVE_PROMOTION_PIECE_MASK 0b00000000000000111000000000000000
#define MOVE_PIECE_MASK           0b00000000000111000000000000000000
#define MOVE_TYPE_MASK            0b00000001111000000000000000000000

#define MOVE_TYPE_SILENT                 0b00000000001000000000000000000000
#define MOVE_TYPE_DOUBLE_PUSH            0b00000000010000000000000000000000
#define MOVE_TYPE_EP                     0b00000000011000000000000000000000
#define MOVE_TYPE_CAPTURE                0b00000000100000000000000000000000
#define MOVE_TYPE_PROMOTION              0b00000000101000000000000000000000
#define MOVE_TYPE_PROMOTION_WITH_CAPTURE 0b00000000110000000000000000000000
#define MOVE_TYPE_CASTLE_K               0b00000000111000000000000000000000
#define MOVE_TYPE_CASTLE_Q               0b00000001000000000000000000000000

#define GET_ORIGIN_SQ(m)       ((m)&MOVE_ORIGIN_MASK)
#define GET_DEST_SQ(m)         (((m)&MOVE_DEST_MASK) >> 6)
#define GET_CAPTURED_PIECE(m)  (((m)&MOVE_CAPTURED_PIECE_MASK) >> 12)
#define GET_PROMOTION_PIECE(m) (((m)&MOVE_PROMOTION_PIECE_MASK) >> 15)
#define GET_PIECE(m)           (((m)&MOVE_PIECE_MASK) >> 18)
#define GET_TYPE(m)            ((m)&MOVE_TYPE_MASK)

#define CREATE_MOVE(o, d, c, pp, p, t) \
  ((o) | ((d) << 6) | ((c) << 12) | ((pp) << 15) | ((p) << 18) | (t))

//
// Types
//

typedef unsigned long long BB;

typedef unsigned long Move;

typedef struct
{
  int           ep_possible;
  BB            ep_square;
  unsigned char castle[2];
  int           halfmove;
  BB            hash_value;
} BoardStateBackup;

typedef struct
{
  Move plies[MAX_MOVES];
  int  plies_count;
} Variation;

typedef struct
{
  int turn;

  BB piece[2][6];
  BB pieces_of[2];
  BB all_pieces;

  unsigned char castle[2];

  int halfmove;
  int fullmove;

  int ep_possible;
  BB  ep_square;

  BB hash_value;

  Variation        variation;
  BoardStateBackup state_backup[MAX_MOVES];
} Board;

typedef struct
{
  int total;
  int eps;
  int castles;
  int promotions;
  int captures;
} PerftResult;

//
// Precomputed values
//
extern const BB precomp_king_moves[64];
extern const BB precomp_knight_moves[64];
extern const BB precomp_files[64];
extern const BB precomp_ranks[64];
extern const BB precomp_rook_blocker_mask[64];
extern const BB precomp_rook_magic[64];
extern const BB precomp_rook_moves[64][4096];
extern const BB precomp_bishop_blocker_mask[64];
extern const BB precomp_bishop_magic[64];
extern const BB precomp_bishop_moves[64][512];
extern const BB precomp_in_between[64][64];
extern const BB precomp_hash[64][12];
extern const BB precomp_hash_castle[2][2];
extern const BB precomp_hash_turn[2];
extern const BB precomp_hash_ep[64];

//
// Functions
//
void Startpos(Board *b);
void FEN(Board *b, char *str);
int  SquareAttackedBy(Board *b, int side, int sq);
int  IsKingAttacked(Board *b, int side);
int  IsLegal(Board *b, Move m);
int  GetPieceAt(Board *b, BB s);
void PrintMoveStr(char buff[], Move m);
void MakeMove(Board *b, Move m);
void UnmakeMove(Board *b);
void PrintBoard(Board *b);
int  GetTotalMaterial(Board *b, int side);
int  IsEndgmae(Board *b);

void Generate(Board *b, int side, int type, Move move_list[], int *move_count);

void RunPerft(Board *b, int depth);
void RunPerftDiv(Board *b, int depth);

Move Search(Board *b);

#endif
