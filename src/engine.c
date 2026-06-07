
#include "log.h"
#include <pthread.h>
#include <stdatomic.h>
#ifdef _WIN32
#define WIN_PIPE_SIZE 4096
#include <io.h>
#else
#include <unistd.h>
#endif

#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <eval.h>

#include "engine.h"
#include "search.h"
#include "cb/cb.h"

#define DEFAULT_FEN "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"

void eng_broadcast_exit(engine_t *eng)
{
    pthread_mutex_lock(&eng->sync_lock);
    atomic_store_explicit(&eng->search_flag, false, memory_order_relaxed);
    atomic_store_explicit(&eng->exit_flag, true, memory_order_relaxed);
    pthread_cond_broadcast(&eng->signal);
    pthread_cond_broadcast(&eng->ready);
    pthread_mutex_unlock(&eng->sync_lock);
}

void eng_broadcast_stop(engine_t *eng)
{
    pthread_mutex_lock(&eng->sync_lock);
    atomic_store_explicit(&eng->search_flag, false, memory_order_relaxed);
    pthread_mutex_unlock(&eng->sync_lock);
}

cibyl_errno_t eng_start_search(cibyl_error_t *err, engine_t *eng, const search_params_t *opts)
{
    cibyl_errno_t result = CIBYL_EOK;

    /* Prepare the engine if anything is not ready. */
    if (eng_prepare(err, eng) != CIBYL_EOK) {
        result = CIBYL_ERR_ADD_CONTEXT(err);
        goto out;
    }

    /* Broadcast the search to the thinkers. */
    pthread_mutex_lock(&eng->sync_lock);
    while (eng->waiting_threads < eng->nthinkers) {
        pthread_cond_wait(&eng->ready, &eng->sync_lock);
    }
    atomic_store_explicit(&eng->search_flag, true, memory_order_relaxed);
    eng->params = *opts;
    pthread_cond_broadcast(&eng->signal);
    pthread_mutex_unlock(&eng->sync_lock);

out:
    return result;
}

/**
 * @brief Reports an error in a thinker thread to the caller via the panic_fd.
 *
 * In UCI, this is necessary to get the calling thread to wake after an error
 * occurs in the engine (potentially caused by a failed tablebase read or malloc
 * failure when allocating the search stack).
 *
 * @param err An error struct containing any error information.
 * @param eng The engine that this thinker belongs to.
 */
cibyl_errno_t thinker_report_error(cibyl_error_t *err, engine_t *eng)
{
    int8_t byte = (int8_t)err->num;
    if (write(eng->error_pipe[1], &byte, 1) != 1)
        return CIBYL_MKERR(err, CIBYL_EABORT, "write: %s\n", strerror(errno));
    return CIBYL_EOK;
}

/**
 * @brief Reports the bestmove found by the engine.
 *
 * Called by the last thinker thread to complete its search.
 * In UCI, this bypasses the caller thread and writes directly to stdout.
 * It seems a shame to have UCI functionality hard coded into the engine
 * itself like this, but the added complexity of doing anything else isn't
 * worth it.
 *
 * @param err An error struct containing any error information.
 * @param eng The engine that this thinker belongs to.
 */
cibyl_errno_t thinker_report_bestmove(cibyl_error_t *err, engine_t *eng, cb_move_t best)
{
    char buf[6];
    cb_mv_to_uci_algbr(buf, eng->bestmove);
    printf("bestmove %s\n", buf);
    return CIBYL_EOK;
}

/**
 * @brief Reports generic info from a thinker thread.
 *
 * Like with the bestmove report function, this is hardcoded to support
 * UCI and bypasses the caller thread. I'm over it now.
 *
 * @param err An error struct containing any error information.
 * @param eng The engine that this thinker belongs to.
 */
cibyl_errno_t thinker_report_info(cibyl_error_t *err, engine_t *eng)
{
    /* TODO: Implement me. */
    printf("Info!\n");
    return CIBYL_EOK;
}

cibyl_errno_t thinker_search(cibyl_error_t *err, thinker_t *tk)
{
    cibyl_errno_t result = CIBYL_EOK;

    /* Load the position into the internal board. */
    if (cb_board_from_copy(err, &tk->board, tk->eng->board) != CIBYL_EOK) {
        result = CIBYL_ERR_ADD_CONTEXT(err);
        goto out;
    }

    /* Complete the search. */
    if (iterative_deepening(err, &tk->eng->bestmove, tk->eng, &tk->board) != CIBYL_EOK) {
        result = CIBYL_ERR_ADD_CONTEXT(err);
        goto out;
    }

out:
    return result;
}

void *thinker_entry(void *thinker_args)
{
    thinker_t *tk = (thinker_t *)thinker_args;
    cibyl_errno_t result = CIBYL_EOK;
    cibyl_error_t err;

    /* Allocate a board to search on. */
    if (cb_board_init(&err, &tk->board) != CIBYL_EOK) {
        result = CIBYL_WRITE_ERR(&err);
        goto out;
    }

    /* Start thinking. */
    while (true) {
        /* Wait the exit flag or for the engine to unset the stop flag. */
        pthread_mutex_lock(&tk->eng->sync_lock);
        tk->eng->waiting_threads += 1;
        if (tk->eng->waiting_threads == tk->eng->nthinkers)
            pthread_cond_signal(&tk->eng->ready);
        while (!tk->eng->exit_flag && !tk->eng->search_flag)
            pthread_cond_wait(&tk->eng->signal, &tk->eng->sync_lock);
        tk->eng->waiting_threads -= 1;
        tk->eng->active_threads += 1;
        pthread_mutex_unlock(&tk->eng->sync_lock);

        /* Don't report if the exit flag is active. */
        if (atomic_load_explicit(&tk->eng->exit_flag, memory_order_relaxed))
            break;

        /* Jump into the search loop. */
        if (thinker_search(&err, tk) != CIBYL_EOK) {
            result = CIBYL_WRITE_ERR(&err);
            goto out;
        }

        /* The last thread should send results back to the caller. */
        if (atomic_fetch_add(&tk->eng->active_threads, -1) == 1) {
            if (thinker_report_bestmove(&err, tk->eng, tk->eng->bestmove)) {
                result = CIBYL_WRITE_ERR(&err);
                goto out;
            }
        }
    }

    /* Free the internal board. */
    cb_board_free(&tk->board);

out:
    /* If there was an error, set the exit flag and signal the caller. */
    if (result != CIBYL_EOK) {
        eng_broadcast_exit(tk->eng);
        thinker_report_error(NULL, tk->eng);
    }

    return (void*)result;
}

void eng_newgame(engine_t *eng)
{
    // TODO: Reset the ttable and stuff when I actually implement this.
}

cibyl_errno_t eng_set_ucifen(cibyl_error_t *err, engine_t *eng, char *fen)
{
    cibyl_errno_t result = CIBYL_EOK;

    /* This may require the board to be initialized. */
    if (eng_prepare(err, eng) != CIBYL_EOK) {
        result = CIBYL_ERR_ADD_CONTEXT(err);
        goto out;
    }

    /* Initialize the board according to the ucifen string. */
    if (cb_board_from_fen(err, eng->board, fen) != CIBYL_EOK) {
        CIBYL_ERR_ADD_CONTEXT(err);
        result = CIBYL_EABORT;
        goto out;
    }

out:
    return result;
}

void eng_notify_ponderhit(engine_t *eng)
{
    // TODO: Implement pondering.
}

void eng_init(engine_t *eng)
{
    /* Mark the board, tables, and thread pool as uninitialized. */
    eng->board = NULL;
    eng->ttable = NULL;
    eng->thinkers = NULL;

    /* Set up sane defaults for hash size (TODO) and thread pool count. */
    eng->nthinkers = 1;

    /* Mark the error pipe as uninitialized. */
#ifdef _WIN32
    /* TODO: Implement for windows. */
#else
    eng->error_pipe[0] = -1;
    eng->error_pipe[1] = -1;
#endif
}

cibyl_errno_t eng_prepare_pipe(cibyl_error_t *err, engine_t *eng)
{
    cibyl_errno_t result = CIBYL_EOK;

    /* Create the panic notification pipe. */
#ifdef _WIN32
    if (CreatePipe(&eng->h_msg_read, &hWritePipe->h_msg_write, NULL, WIN_PIPE_SIZE)
        result = CIBYL_MKERR(err, CIBYL_EABORT, "CreatePipe: %s\n", _strerror(NULL));
#else
    if (pipe(eng->error_pipe) == -1)
        result = CIBYL_MKERR(err, CIBYL_EABORT, "pipe: %s\n", strerror(errno));
#endif

    return result;
}

void eng_cleanup_pipe(engine_t *eng)
{
    close(eng->error_pipe[0]);
    close(eng->error_pipe[1]);
}

cibyl_errno_t eng_prepare_board(cibyl_error_t *err, engine_t *eng)
{
    cibyl_errno_t result = CIBYL_EOK;
    char fen[] = DEFAULT_FEN;

    if ((eng->board = (cb_board_t*)malloc(sizeof(cb_board_t))) == NULL) {
        result = CIBYL_MKERR(err, CIBYL_ENOMEM, "malloc: %s\n", strerror(errno));
        goto out;
    }

    if (cb_tables_init(err) != CIBYL_EOK) {
        result = CIBYL_ERR_ADD_CONTEXT(err);
        goto err_deinit_board;
    }

    if (cb_board_init(err, eng->board) != CIBYL_EOK) {
        result = CIBYL_ERR_ADD_CONTEXT(err);
        goto err_free_board;
    }

    if (cb_board_from_fen(err, eng->board, fen) != CIBYL_EOK) {
        result = CIBYL_ERR_ADD_CONTEXT(err);
        goto err_deinit_board;
    }

    goto out;

err_deinit_board:
    cb_board_free(eng->board);
err_free_board:
    free(eng->board);
    eng->board = NULL;

out:
    return result;
}

void eng_cleanup_board(engine_t *eng)
{
    cb_tables_free();
    cb_board_free(eng->board);
    free(eng->board);
}

cibyl_errno_t eng_prepare_ttable(engine_t *eng)
{
    cibyl_errno_t result = CIBYL_EOK;

    /* Allocate memory for the thread pool. */
    if ((eng->ttable = (ttable_t*)malloc(sizeof(ttable_t))) == NULL) {
        cibyl_write_log("malloc: %s\n", strerror(errno));
        result = CIBYL_ENOMEM;
        goto out;
    }

out:
    return result;
}

void eng_cleanup_ttable(engine_t *eng)
{
    free(eng->ttable);
}

cibyl_errno_t eng_prepare_thinkers(engine_t *eng)
{
    cibyl_errno_t result = CIBYL_EOK;
    int presult = 0;
    int i;

    /* Allocate space for the thinker pool. */
    if ((eng->thinkers = (thinker_t*)malloc(eng->nthinkers * sizeof(thinker_t))) == NULL) {
        cibyl_write_log("malloc: %s\n", strerror(errno));
        goto out;
    }

    /* Initialize synchronization primitives. */
    pthread_mutex_init(&eng->sync_lock, NULL);
    pthread_cond_init(&eng->signal, NULL);
    pthread_cond_init(&eng->ready, NULL);
    atomic_store_explicit(&eng->exit_flag, false, memory_order_relaxed);
    atomic_store_explicit(&eng->search_flag, false, memory_order_relaxed);
    eng->waiting_threads = 0;
    eng->active_threads = 0;

    /* Spawn the thinker threads. */
    for (i = 0; i < eng->nthinkers; i++) {
        eng->thinkers[i].eng = eng;
        eng->thinkers[i].root = eng->board;
        eng->thinkers[i].ttable = eng->ttable;
        eng->thinkers[i].tid = i;
        if ((presult = pthread_create(&eng->thinkers[i].thread, NULL,
                                      thinker_entry, &eng->thinkers[i])) != 0) {
            cibyl_write_log("pthread_create: %s\n", strerror(errno));
            result = CIBYL_EABORT;
            goto err_join;
        }
    }

    goto out;

err_join:
    eng_broadcast_exit(eng);
    for (; i > 0; i--) {
        pthread_join(eng->thinkers[i].thread, NULL);
    }

    pthread_cond_destroy(&eng->ready);
    pthread_cond_destroy(&eng->signal);
    pthread_mutex_destroy(&eng->sync_lock);
err_free:
    free(eng->ttable);
    eng->ttable = NULL;

out:
    return result;
}

void eng_cleanup_thinkers(engine_t *eng)
{
    eng_broadcast_exit(eng);
    for (int i = 0; i < eng->nthinkers; i++) {
        pthread_join(eng->thinkers[i].thread, NULL);
    }
    pthread_cond_destroy(&eng->ready);
    pthread_cond_destroy(&eng->signal);
    pthread_mutex_destroy(&eng->sync_lock);
}

cibyl_errno_t eng_prepare(cibyl_error_t *err, engine_t *eng)
{
    cibyl_errno_t result = CIBYL_EOK;

    /* Initialize the error pipe. */
    if (eng->error_pipe[0] == -1 && eng_prepare_pipe(err, eng)) {
        result = CIBYL_ERR_ADD_CONTEXT(err);
        goto err;
    }
    
    /* Potentially initialize the board and move tables. */
    if (eng->board == NULL && eng_prepare_board(err, eng) != CIBYL_EOK) {
        result = CIBYL_ERR_ADD_CONTEXT(err);
        goto err;
    }

    /* Potentially initialize the ttable. */
    if (eng->ttable == NULL && eng_prepare_ttable(eng) != CIBYL_EOK) {
        result = CIBYL_ERR_ADD_CONTEXT(err);
        goto err;
    }

    /* Potentially inititialize the thread pool. */
    if (eng->thinkers == NULL && eng_prepare_thinkers(eng) != CIBYL_EOK) {
        result = CIBYL_ERR_ADD_CONTEXT(err);
        goto err;
    }

err:
    return result;
}

void eng_deinit(engine_t *eng)
{
    /* Potentially cleanup the error pipe. */
    if (eng->error_pipe[0] != -1) {
        eng_cleanup_pipe(eng);
        eng->error_pipe[0] = -1;
        eng->error_pipe[1] = -1;
    }

    /* Potentially cleanup the thread pool. */
    if (eng->thinkers != NULL) {
        eng_cleanup_thinkers(eng);
        eng->thinkers = NULL;
    }

    /* Potentially cleanup the ttable. */
    if (eng->ttable != NULL) {
        eng_cleanup_ttable(eng);
        eng->ttable = NULL;
    }

    /* Potentially cleanup the board and move tables. */
    if (eng->board != NULL) {
        eng_cleanup_board(eng);
        eng->board = NULL;
    }
}
