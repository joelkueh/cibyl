
#include "engine.h"

/* This is just a minimax search for now. I'll flesh it out into alphabeta later. */
float searching(cb_board_t *board, cb_state_tables_t *state, int depth, bool *search_flag)
{
    cb_mvlst_t mvlst;
    cb_move_t mv;
    float value = FLT_MAX;
    float result;
    int i;

    /* Perform static evaluation if we are ready to evaluate as is. */
    if (depth < 1)
        return eval(board);

    /* Make moves down the tree. */
    cb_gen_board_tables(state, board);
    cb_gen_moves(&mvlst, board, state);
    for (i = 0; i < cb_mvlst_size(&mvlst); i++) {
        if (!*search_flag)
            return 0.0;
        mv = cb_mvlst_at(&mvlst, i);
        cb_make(board, mv);
        result = searching(board, state, depth - 1, search_flag);
        value = board->turn ? MAX(result, value) : MIN(result, value);
        cb_unmake(board);
    }

    return value;
}

cb_move_t alphabeta(cb_board_t *board, int base_depth, bool *search_flag)
{
    cb_errno_t result;
    cb_error_t err;

    cb_state_tables_t state;
    cb_mvlst_t mvlst;
    cb_move_t best_move;
    cb_move_t mv;

    float best_eval = FLT_MIN;
    float eval;
    int i;

    /* Exit early if depth is less than 1. */
    if (base_depth < 1) {
        printf("Can't alphabeta with a depth below 1\n");
        return CB_INVALID_MOVE;
    }

    /* Reserve the board history up to ALPHA_BETA_MAX_DEPTH. */
    if ((result = cb_reserve_for_make(&err, board, ALPHA_BETA_MAX_DEPTH)) != 0) {
        fprintf(stderr, "cb_reserve_for_make: %s\n", err.desc);
        return CB_INVALID_MOVE;
    }

    /* Start the alpha-beta search. */
    cb_gen_board_tables(&state, board);
    cb_gen_moves(&mvlst, board, &state);
    for (i = 0; i < cb_mvlst_size(&mvlst); i++) {
        if (!*search_flag)
            return 0.0;
        mv = cb_mvlst_at(&mvlst, i);
        cb_make(board, mv);
        eval = searching(board, &state, base_depth - 1, search_flag);
        if (eval > best_eval) {
            best_eval = eval;
            best_move = mv;
        }
        cb_unmake(board);
    }

    return best_move;
}

cb_move_t iterative_deepening(cb_board_t *board, bool *search_flag)
{
    cb_move_t best_move;
    cb_move_t result;
    int i;
    
    /* Perform the search with iterative deepening. */
    for (i = 1; i < ALPHA_BETA_MAX_DEPTH; i++) {
        /* TODO: Reuse the previous move ordering for this search. */
        result = alphabeta(board, i, search_flag);
        if (!*search_flag)
            return best_move;
        best_move = result;
    }

    return best_move;
}
