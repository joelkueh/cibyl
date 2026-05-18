
#include <pthread.h>
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
#include "engine.h"

/**
 * @breif Handles errors in a thinker thread.
 *
 * If a thinker encounters an error, it must set the atomic termination flag,
 * log the information to the console, and exit.
 *
 * @param eng The engine that the thinker belongs to.
 * @param format A printf compliant format string.
 * @param ... All remaining arguments passed into the format string.
 */
void thinker_panic(engine_t *eng, char *format, ...)
{
    va_list args;
    atomic_store_explicit(&eng->exit_flag, true, memory_order_relaxed);
    va_start(args, format);
    cibyl_vwrite_log(format, args);
    va_end(args);
    if (eng->report_error != NULL)
        eng->report_error(eng);
    pthread_exit((void*)1);
}

void thinker_handle_go(thinker_t *tk)
{

}

void *thinker_entry(void *thinker_args)
{
    thinker_t *tk = (thinker_t *)thinker_args;
    cibyl_errno_t result = CIBYL_EOK;

    /* Start thinking. */
    while (true) {
        /* Sync all threads with the UI thread on the start barrier (N+1). */
        pthread_barrier_wait(&tk->eng->start);
        if (atomic_load_explicit(&tk->eng->exit_flag, memory_order_relaxed) == true)
            break;

        /* Sync threads within the pool (N) on the end barrier after search. */
        thinker_handle_go(tk);
        pthread_barrier_wait(&tk->eng->end);

        /* The manager thread can send results back to the caller. */
        if (tk->tid == 0 && tk->eng->report_best != NULL)
            tk->eng->report_best(tk->eng);
    }

out:
    return (void*)result;
}

/**
 * @brief Performs engine-wide initialization steps.
 * @param The engine to initialize.
 */
cibyl_errno_t manager_engine_init(engine_t *eng)
{
    cb_error_t cb_err;
    cb_errno_t cb_errno;
    cibyl_errno_t result;
    int presult;

    /* Initialize the board. */
    if (cb_board_init(&cb_err, &eng->board) != CB_EOK) {
        cibyl_write_log("cb_board_init: %s\n", cb_err.desc);
        result = CIBYL_EABORT;
        goto out;
    }

    if (cb_tables_init(&cb_err) != CB_EOK) {
        cibyl_write_log("cb_tables_init: %s\n", cb_err.desc);
        result = CIBYL_EABORT;
        goto err_cleanup_board;
    }

err_destroy_start:
    pthread_barrier_destroy(&eng->start);
err_cleanup_tables:
    cb_tables_free();
err_cleanup_board:
    cb_board_free(&eng->board);

out:
    return result;
}

cibyl_errno_t manager_run_pool(thinker_t *tk)
{
    cibyl_errno_t result;
    thinker_t *tkrs;
    int presult;
    int i;

    /* Allocate memory for the other thinkers. */
    if ((tkrs = (thinker_t*)malloc((tk->eng->nthinkers - 1) * sizeof(thinker_t))) == NULL) {
        cibyl_write_log("malloc: %s\n", strerror(errno));
        result = CIBYL_EABORT;
        goto err;
    }

    /* Become a thinker. */
    result = (intptr_t)thinker_entry(tk);

    /* Cleanup. */
err_join:
    for (; i > 0; i--) {
        pthread_cancel(tkrs[i].thread);
        pthread_join(tkrs[i].thread, NULL);
    }
    free(tkrs);
err:
    
    return result;
}

/**
 * @brief Performs engine-wide deinitialization steps.
 * @param The engine to deinitialize.
 */
void manager_engine_deinit(engine_t *eng)
{
    cb_board_free(&eng->board);
    cb_tables_free();
}

void *manager_entry(void *thinker_args)
{
    thinker_t *tk = (thinker_t *)thinker_args;
    intptr_t result = CIBYL_EOK;

    /* Initialize the engine. */
    if (manager_engine_init(tk->eng)) {
        result = CIBYL_EABORT;
        goto err;
    }

    /* Run the thinker pool in a loop. */
    // while (true) {
    //     if (
    // }
    
err_cleanup_engine:
    manager_engine_deinit(tk->eng);
err:

    /* Report error if something went wrong. */
    if (result != 0 && tk->eng->report_error)
        tk->eng->report_error(tk->eng);

    return (void*)result;
}

cibyl_errno_t eng_begin_init(engine_t *eng_addr)
{
    engine_t *eng = (engine_t *)eng_addr;
    cibyl_errno_t result = CIBYL_EOK;
    int presult;
    int i;

    /* Spawn the manager of the thinker pool. Manager will spawn remaining threads. */
    eng->manager.root = &eng->board;
    eng->manager.ttable = &eng->ttable;
    eng->manager.eng = eng;
    eng->manager.tid = 0;
    if ((presult = pthread_create(&eng->manager.thread, NULL, thinker_entry, eng_addr)) != 0) {
        cibyl_write_log("pthread_create: %s\n", strerror(presult));
        result = CIBYL_EABORT;
    }

    return result;
}

void eng_cleanup(engine_t *eng)
{
    int i;
    pthread_join(eng->manager.thread, NULL);
    cb_tables_free();
    cb_board_free(&eng->board);
}

void eng_newgame(engine_t *eng)
{
    // TODO: Reset the ttable and stuff when I actually implement this.
}

cibyl_errno_t eng_set_ucifen(engine_t *eng, char *fen)
{
    cb_error_t cb_err;
    cb_errno_t cb_errno;

    if ((cb_errno = cb_board_from_fen(&cb_err, &eng->board, fen)) == CB_EOK) {
        cibyl_write_log("cb_board_from_fen: %s\n", cb_err);
        return CIBYL_EABORT;
    }

    return CIBYL_EOK;
}

void eng_start_search(engine_t *eng, const search_params_t *opts)
{
    eng->params = *opts;
    atomic_store_explicit(&eng->stop_flag, false, memory_order_relaxed);
    pthread_barrier_wait(&eng->start);
}

void eng_notify_stop(engine_t *eng)
{
    atomic_store_explicit(&eng->stop_flag, true, memory_order_relaxed);
}

void eng_notify_ponderhit(engine_t *eng)
{
    // TODO: Implement pondering.
}

void eng_get_result(engine_t *eng)
{
    // TODO: Collect the result from the ttable once implemented.
}

void eng_init(engine_t *eng)
{
    /* Mark the board, tables, and thread pool as uninitialized. */
    eng->board = NULL;
    eng->ttable = NULL;
    eng->thinkers = NULL;
}

cibyl_errno_t eng_prepare_board(engine_t *eng)
{
    cibyl_errno_t result = CIBYL_EOK;
    cb_error_t cb_error;
    cb_errno_t cb_errno;

    if ((eng->board = (cb_board_t*)malloc(sizeof(cb_board_t))) == NULL) {
        cibyl_write_log("malloc: %s\n", strerror(errno));
        goto out;
    }

    if ((cb_errno = cb_board_init(&cb_error, eng->board)) != CB_EOK) {
        cibyl_write_log("cb_board_init: %s\n", cb_error.desc);
        goto err_free_board;
    }

    if ((cb_errno = cb_tables_init(&cb_error)) != CB_EOK) {
        cibyl_write_log("cb_board_init: %s\n", cb_error.desc);
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
    eng->board = NULL;
}

cibyl_errno_t eng_prepare_ttable(engine_t *eng)
{
    cibyl_errno_t result = CIBYL_EOK;
    int presult = 0;
    int i;

    /* Allocate memory for the thread pool. */
    if ((eng->ttable = (ttable_t*)malloc(sizeof(ttable_t))) == NULL) {
        cibyl_write_log("malloc: %s\n", strerror(errno));
        result = CIBYL_ENOMEM;
        goto out;
    }

    /* Initialize synchronization primitives. */
    if ((presult = pthread_barrier_init(&eng->start, NULL, eng->nthinkers + 1)) != 0) {
        cibyl_write_log("pthread_barrier_init: %s\n", strerror(presult));
        result = CIBYL_EABORT;
        goto err_free;
    };

    if ((presult = pthread_barrier_init(&eng->end, NULL, eng->nthinkers + 1)) != 0) {
        cibyl_write_log("pthread_barrier_init: %s\n", strerror(presult));
        result = CIBYL_EABORT;
        goto err_destroy_start;
    };

    /* Spawn the thinker threads. */
    for (i = 0; i < eng->nthinkers - 1; i++) {
        eng->thinkers[i].eng = eng;
        eng->thinkers[i].root = eng->board;
        eng->thinkers[i].ttable = eng->ttable;
        eng->thinkers[i].tid = i+1;
        if ((presult = pthread_create(&eng->thinkers[i].thread, NULL,
                                      thinker_entry, &eng->thinkers[i])) != 0) {
            cibyl_write_log("pthread_create: %s\n", strerror(errno));
            result = CIBYL_EABORT;
            goto err_join;
        }
    }

    goto out;

err_join:
    for (; i > 0; i--) {
        pthread_cancel(eng->thinkers[i].thread);
        pthread_join(eng->thinkers[i].thread, NULL);
    }

    pthread_barrier_destroy(&eng->end);
err_destroy_start:
    pthread_barrier_destroy(&eng->start);
err_free:
    free(eng->ttable);
    eng->ttable = NULL;

out:
    return result;
}

void eng_cleanup_ttable(engine_t *eng)
{
    free(eng->ttable);
    eng->ttable = NULL;
}

cibyl_errno_t eng_prepare_thinkers(engine_t *eng)
{
    cibyl_errno_t result = CIBYL_EOK;

    if ((eng->thinkers = (thinker_t*)malloc(eng->nthinkers * sizeof(thinker_t))) == NULL) {
        cibyl_write_log("malloc: %s\n", strerror(errno));
        goto err;
    }

err:
    return result;
}

void eng_cleanup_thinkers(engine_t *eng)
{

}

cibyl_errno_t eng_prepare(engine_t *eng)
{
    cibyl_errno_t result = CIBYL_EOK;
    
    /* Potentially initialize the board and move tables. */
    if (eng->board == NULL) {
    }

    /* Potentially initialize the ttable. */
    if (eng->ttable == NULL && (result = eng_prepare_ttable(eng)) != CIBYL_EOK) {
        goto err;
    }

    /* Potentially inititialize the thread pool. */
    if (eng->thinkers == NULL && (result = eng_prepare_thinkers(eng)) != CIBYL_EOK) {
        goto err;
    }

err:
}

void eng_deinit(engine_t *eng)
{
    /* Potentially cleanup the board and move tables. */
    if (eng->board != NULL) {
        cb_board_free(eng->board);
        cb_board_
    }

}
