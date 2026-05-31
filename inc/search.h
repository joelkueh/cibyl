
#ifndef CIBYL_SEARCH_H
#define CIBYL_SEARCH_H

#include "cb/types.h"
#include "engine.h"
#include "log.h"

/**
 * Performs an iterative deepneing bestmove search.
 */
cibyl_errno_t iterative_deepening(cibyl_error_t *err, cb_move_t *bestmove,
                                  engine_t *eng, cb_board_t *baord);

#endif /* CIBYL_SEARCH_H */
