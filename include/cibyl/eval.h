
#ifndef CYBIL_EVAL_H
#define CYBIL_EVAL_H

#include <stdbool.h>
#include "cb_types.h"

/**
 * Evaluates the position specified by board.
 */
float eval(const cb_board_t *board);

/**
 * Performs an iterative deepneing bestmove search.
 */
cb_move_t iterative_deepening(cb_board_t *board, bool *cancel_token);

#endif /* CIBYL_EVAL_H */
