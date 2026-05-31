
#include <float.h>

#include "eval.h"
#include "cb/bitutil.h"

float piece_differential(const cb_board_t *board)
{
    float diff = 0.0f;
    
    if (board->bb.piece[board->turn][CB_PTYPE_KING] == 0)
        return board->turn ? FLT_MIN : FLT_MAX;

    diff += popcnt(board->bb.piece[board->turn][CB_PTYPE_PAWN]) * 1.0f;
    diff -= popcnt(board->bb.piece[!board->turn][CB_PTYPE_PAWN]) * 1.0f;
    diff += popcnt(board->bb.piece[board->turn][CB_PTYPE_KNIGHT]) * 3.0f;
    diff -= popcnt(board->bb.piece[!board->turn][CB_PTYPE_KNIGHT]) * 3.0f;
    diff += popcnt(board->bb.piece[board->turn][CB_PTYPE_BISHOP]) * 3.0f;
    diff -= popcnt(board->bb.piece[!board->turn][CB_PTYPE_BISHOP]) * 3.0f;
    diff += popcnt(board->bb.piece[board->turn][CB_PTYPE_ROOK]) * 5.0f;
    diff -= popcnt(board->bb.piece[!board->turn][CB_PTYPE_ROOK]) * 5.0f;
    diff += popcnt(board->bb.piece[board->turn][CB_PTYPE_QUEEN]) * 9.0f;
    diff -= popcnt(board->bb.piece[!board->turn][CB_PTYPE_QUEEN]) * 9.0f;

    return diff;
}

float eval(const cb_board_t *board)
{
    return piece_differential(board);
}

