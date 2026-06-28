
#ifndef CIBYL_PERFT_H
#define CIBYL_PERFT_H

#include "cb/types.h"
#include "log.h"

cibyl_errno_t perft(cibyl_error_t *err, cb_board_t *board, int depth);
cibyl_errno_t perft_cheat(cibyl_error_t *err, cb_board_t *board, int depth);

#endif /* CIBYL_PERFT_H */
