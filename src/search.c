
#include <float.h>

#include "engine.h"
#include "cb/cb.h"
#include "cb/move.h"
#include "eval.h"
#include <stdatomic.h>

#define ALPHA_BETA_MAX_DEPTH 30

#define MIN(x, y) (x > y ? y : x)
#define MAX(x, y) (x > y ? x : y)

/* This is just a minimax search for now. I'll flesh it out into alphabeta later. */
cibyl_errno_t searching(cibyl_error_t *err, float *evaluation,
                        engine_t *eng, cb_board_t *board, cb_state_tables_t *state, int depth)
{
    cibyl_errno_t result = CIBYL_EOK;
    cb_mvlst_t mvlst;
    cb_move_t mv;
    float value = FLT_MAX;
    float current_eval;
    int i;

    /* Perform static evaluation if we are ready to evaluate as is. */
    if (depth < 1) {
        result = CIBYL_EOK;
        value = eval(board);
        goto out;
    }

    /* Make moves down the tree. */
    cb_gen_board_tables(state, board);
    cb_gen_moves(&mvlst, board, state);
    for (i = 0; i < cb_mvlst_size(&mvlst); i++) {
        /* Check the search flag to see if we should stop. */
        if (!atomic_load_explicit(&eng->search_flag, memory_order_relaxed)) {
            result = CIBYL_EOK;
            value = 0.0;
            goto out;
        }

        /* Make another move and explore the search tree recursively. */
        mv = cb_mvlst_at(&mvlst, i);
        cb_make(board, mv);
        current_eval = searching(err, &current_eval, eng, board, state, depth - 1);
        value = board->turn ? MAX(current_eval, value) : MIN(current_eval, value);
        cb_unmake(board);
    }

out:
    *evaluation = value;
    return value;
}

cibyl_errno_t alphabeta(cibyl_error_t *err, cb_move_t *bestmove,
                        engine_t *eng, cb_board_t *board, int base_depth)
{
    cibyl_errno_t result = CIBYL_EOK;
    cb_state_tables_t state;
    cb_mvlst_t mvlst;
    cb_move_t current_bestmove;
    cb_move_t mv;

    float best_eval = FLT_MIN;
    float eval;
    int i;

    /* Exit early if depth is less than 1. */
    if (base_depth < 1) {
        result = CIBYL_MKERR(err, CIBYL_EINVAL, "invalid alphabeta search depth\n");
        goto out;
    }

    /* Reserve the board history up to ALPHA_BETA_MAX_DEPTH. */
    if (cb_reserve_for_make(NULL, board, ALPHA_BETA_MAX_DEPTH) != 0) {
        result = CIBYL_ERR_ADD_CONTEXT(err);
        goto out;
    }

    /* Start the alpha-beta search. */
    cb_gen_board_tables(&state, board);
    cb_gen_moves(&mvlst, board, &state);
    for (i = 0; i < cb_mvlst_size(&mvlst); i++) {
        /* Check the atomic stop flag. */
        if (!atomic_load_explicit(&eng->search_flag, memory_order_relaxed)) {
            result = CIBYL_EOK;
            goto out;
        }

        /* Make the move and search recursively. */
        mv = cb_mvlst_at(&mvlst, i);
        cb_make(board, mv);
        if (searching(err, &eval, eng, board, &state, base_depth - 1) != CIBYL_EOK) {
            result = CIBYL_ERR_ADD_CONTEXT(err);
            goto out;
        }

        /* Collect the results and unmake the move. */
        if (eval > best_eval) {
            best_eval = eval;
            current_bestmove = mv;
        }
        cb_unmake(board);
    }

out:
    *bestmove = current_bestmove;
    return result;
}

cibyl_errno_t iterative_deepening(cibyl_error_t *err, cb_move_t *bestmove,
                                  engine_t *eng, cb_board_t *board)
{
    cibyl_errno_t result = CIBYL_EOK;
    cb_move_t current_bestmove = CB_INVALID_MOVE;
    cb_move_t mv;
    int i;
    
    /* Perform the search with iterative deepening. */
    for (i = 1; i < ALPHA_BETA_MAX_DEPTH; i++) {
        /* TODO: Reuse the previous move ordering for this search. */
        if (alphabeta(err, &mv, eng, board, i) != CIBYL_EOK) {
            result = CIBYL_ERR_ADD_CONTEXT(err);
            goto out;
        }

        /* Check the exit flag as the stop condition. */
        if (!atomic_load_explicit(&eng->search_flag, memory_order_relaxed))
            goto out;
        current_bestmove = mv;
    }

out:
    *bestmove = current_bestmove;
    return result;
}
