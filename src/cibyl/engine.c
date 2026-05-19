
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

void eng_broadcast_exit(engine_t *eng)
{
    pthread_mutex_lock(&eng->sync_lock);
    eng->search_flag = false;
    eng->exit_flag = true;
    pthread_cond_broadcast(&eng->signal);
    pthread_cond_broadcast(&eng->ready);
    pthread_mutex_unlock(&eng->sync_lock);
}

void eng_broadcast_stop(engine_t *eng)
{
    pthread_mutex_lock(&eng->sync_lock);
    eng->search_flag = false;
    pthread_mutex_unlock(&eng->sync_lock);
}

cibyl_errno_t eng_start_search(engine_t *eng, const search_params_t *opts)
{
    cibyl_errno_t result = CIBYL_EOK;

    /* Prepare the engine if anything is not ready. */
    if (eng_prepare(eng)) {
        result = CIBYL_EABORT;
        goto out;
    }

    /* Broadcast the search to the thinkers. */
    pthread_mutex_lock(&eng->sync_lock);
    while (eng->waiting_threads < eng->nthinkers) {
        pthread_cond_wait(&eng->ready, &eng->sync_lock);
    }
    eng->search_flag = true;
    eng->params = *opts;
    pthread_cond_broadcast(&eng->signal);
    pthread_mutex_unlock(&eng->sync_lock);

out:
    return result;
}

void eng_register_error(engine_t *eng, cibyl_errno_t (*report_fn)(engine_t *eng, void *udata))
{
    eng->report_error = report_fn;
}

void eng_register_best(engine_t *eng, cibyl_errno_t (*report_fn)(engine_t *eng, void *udata))
{
    eng->report_best = report_fn;
}

void eng_register_info(engine_t *eng, cibyl_errno_t (*report_fn)(engine_t *eng, void *udata))
{
    eng->report_info = report_fn;
}

/**
 * @breif Handles errors in a thinker thread.
 *
 * Signals all other theads to exit, notifies the caller,
 * and writes a message to the log (often stderr).
 *
 * @param eng The engine that the thinker belongs to.
 * @param format A printf compliant format string.
 * @param ... All remaining arguments passed into the format string.
 */
void thinker_error(engine_t *eng, char *format, ...)
{
    va_list args;
    eng_broadcast_exit(eng);
    va_start(args, format);
    cibyl_vwrite_log(format, args);
    va_end(args);
    if (eng->report_error != NULL) {
        if (eng->report_error(eng, eng->udata)) {
            cibyl_write_log("report_error failed\n");
        }
    }
}

void thinker_handle_go(thinker_t *tk)
{
    /* TODO: Think. */
}

void *thinker_entry(void *thinker_args)
{
    thinker_t *tk = (thinker_t *)thinker_args;
    cibyl_errno_t result = CIBYL_EOK;

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

        /* Jump into the search loop. */
        thinker_handle_go(tk);

        /* Don't report if the exit flag is active. */
        if (tk->eng->exit_flag)
            break;

        /* The last thread should send results back to the caller. */
        if (atomic_fetch_add(&tk->eng->active_threads, -1) == 1) {
            if (tk->eng->report_best(tk->eng, tk->eng->udata)) {
                cibyl_write_log("report_best failed");
            }
        }
    }

out:
    return (void*)result;
}

void eng_newgame(engine_t *eng)
{
    // TODO: Reset the ttable and stuff when I actually implement this.
}

cibyl_errno_t eng_set_ucifen(engine_t *eng, char *fen)
{
    cibyl_errno_t result = CIBYL_EOK;
    cb_error_t cb_err;
    cb_errno_t cb_errno;

    /* This may require the board to be initialized. */
    if (eng_prepare(eng)) {
        result = CIBYL_EABORT;
        goto out;
    }

    /* Initialize the board according to the ucifen string. */
    if ((cb_errno = cb_board_from_fen(&cb_err, eng->board, fen)) == CB_EOK) {
        cibyl_write_log("cb_board_from_fen: %s\n", cb_err);
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
    eng->exit_flag = false;
    eng->search_flag = false;
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
    for (int i = eng->nthinkers; i > 0; i--) {
        pthread_join(eng->thinkers[i].thread, NULL);
    }
    pthread_cond_destroy(&eng->ready);
    pthread_cond_destroy(&eng->signal);
    pthread_mutex_destroy(&eng->sync_lock);
}

cibyl_errno_t eng_prepare(engine_t *eng)
{
    cibyl_errno_t result = CIBYL_EOK;
    
    /* Potentially initialize the board and move tables. */
    if (eng->board == NULL && (result = eng_prepare_board(eng)) != CIBYL_EOK) {
        goto err;
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
    return result;
}

void eng_deinit(engine_t *eng)
{
    /* Potentially cleanup the board and move tables. */
    if (eng->board != NULL) {
        eng_cleanup_board(eng);
        eng->board = NULL;
    }

    /* Potentially cleanup the ttable and move tables. */
    if (eng->ttable != NULL) {
        eng_cleanup_ttable(eng);
        eng->ttable = NULL;
    }

    /* Potentially cleanup the thread pool. */
    if (eng->thinkers != NULL) {
        eng_cleanup_thinkers(eng);
        eng->thinkers = NULL;
    }
}
