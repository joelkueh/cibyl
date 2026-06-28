
#include <time.h>

#include "cb/cb.h"
#include "cb/move.h"
#include "cb/types.h"
#include <inttypes.h>

uint64_t perfting(cb_board_t *board, int depth)
{
    uint64_t cnt = 0;
    int i;
    cb_move_t mv;
    cb_mvlst_t mvlst;

    /* Base case. */
    if (depth <= 0)
        return 1;

    /* Generate the moves. */
    cb_gen_moves(&mvlst, board);

    /* Make moves and move down the tree. */
    for (i = 0; i < cb_mvlst_size(&mvlst); i++) {
        mv = cb_mvlst_at(&mvlst, i);
        /* This function can fail, but only when a reservation is needed.
         * As perft does a manual reservation, there is no need to reserve here and no error. */
        cb_make(board, mv);

        /*cb_print_bitboard(stdout, board);
        cb_print_mv_hist(stdout, board);*/

        cnt += perfting(board, depth - 1);
        cb_unmake(board);
    }

    return cnt;
}

uint64_t perft_cheating(cb_board_t *board, int depth)
{
    uint64_t cnt = 0;
    int i;
    cb_move_t mv;
    cb_mvlst_t mvlst;

    /* Generate the moves. */
    cb_gen_moves(&mvlst, board);

    /* Base case. */
    if (depth <= 1)
        return cb_mvlst_size(&mvlst);

    /* Make moves and move down the tree. */
    for (i = 0; i < cb_mvlst_size(&mvlst); i++) {
        mv = cb_mvlst_at(&mvlst, i);
        /* This function can fail, but only when a reservation is needed.
         * As perft does a manual reservation, there is no need to reserve here and no error. */
        cb_make(board, mv);

        /*cb_print_bitboard(stdout, board);
        cb_print_mv_hist(stdout, board);*/

        cnt += perft_cheating(board, depth - 1);
        cb_unmake(board);
    }

    return cnt;
}

cibyl_errno_t perft(cibyl_error_t *err, cb_board_t *board, int depth)
{
    cibyl_errno_t result = CIBYL_EOK;
    cb_mvlst_t mvlst;
    cb_move_t mv;
    uint64_t perft_results[CB_MAX_NUM_MOVES];
    uint64_t cnt = 0;
    uint64_t total = 0;
    char buf[6];
    int i;

    struct timespec start;
    struct timespec end;
    double elapsed;

    /* Exit early if depth is less than 1. */
    if (depth < 1) {
        result = CIBYL_MKERR(err, CIBYL_EINVAL, "illegal depth %d specified for perft", depth);
        goto out;
    }

    /* Reserve the board history up to maximum perft depth. */
    if ((result = cb_reserve_for_make(err, board, depth)) != 0) {
        CIBYL_ERR_ADD_CONTEXT(err);
        goto out;
    }

    /* Loop through all of the first levels and calculate the number of moves. */
    timespec_get(&start, TIME_UTC);
    cb_gen_moves(&mvlst, board);
    for (i = 0; i < cb_mvlst_size(&mvlst); i++) {
        mv = cb_mvlst_at(&mvlst, i);
        cb_make(board, mv);
        cnt = perfting(board, depth - 1);
        total += cnt;
        cb_mv_to_uci_algbr(buf, mv);
        printf("%s: %" PRIu64 "\n", buf, cnt);
        cb_unmake(board);
    }
    timespec_get(&end, TIME_UTC);
    elapsed = (double)(end.tv_sec - start.tv_sec) + (double)(end.tv_nsec - start.tv_nsec) / 1.0e9;
    printf("\n");
    printf("Nodes searched: %" PRIu64 "\n", total);
    printf("Time: %.3f ms\n", elapsed * 1.0e3);
    printf("\n");

out:
    return result;
}

cibyl_errno_t perft_cheat(cibyl_error_t *err, cb_board_t *board, int depth)
{
    cibyl_errno_t result = CIBYL_EOK;
    cb_mvlst_t mvlst;
    cb_move_t mv;
    uint64_t perft_results[CB_MAX_NUM_MOVES];
    uint64_t cnt = 0;
    uint64_t total = 0;
    char buf[6];
    int i;

    struct timespec start;
    struct timespec end;
    double elapsed;

    /* Exit early if depth is less than 1. */
    if (depth < 1) {
        result = CIBYL_MKERR(err, CIBYL_EINVAL, "illegal depth %d specified for perft", depth);
        goto out;
    }

    /* Reserve the board history up to maximum perft depth. */
    if ((result = cb_reserve_for_make(err, board, depth)) != 0) {
        CIBYL_ERR_ADD_CONTEXT(err);
        goto out;
    }

    /* Loop through all of the first levels and calculate the number of moves. */
    timespec_get(&start, TIME_UTC);
    cb_gen_moves(&mvlst, board);
    for (i = 0; i < cb_mvlst_size(&mvlst); i++) {
        mv = cb_mvlst_at(&mvlst, i);
        cb_make(board, mv);
        if (depth > 1)
            cnt = perft_cheating(board, depth - 1);
        else
            cnt = 1;
        total += cnt;
        cb_mv_to_uci_algbr(buf, mv);
        printf("%s: %" PRIu64 "\n", buf, cnt);
        cb_unmake(board);
    }
    timespec_get(&end, TIME_UTC);
    elapsed = (double)(end.tv_sec - start.tv_sec) + (double)(end.tv_nsec - start.tv_nsec) / 1.0e9;
    printf("\n");
    printf("Nodes searched: %" PRIu64 "\n", total);
    printf("Time: %.3f ms\n", elapsed * 1.0e3);
    printf("\n");

out:
    return result;
}
