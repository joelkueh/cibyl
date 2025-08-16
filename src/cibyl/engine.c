
#ifdef _WIN32
#define WIN_PIPE_SIZE 4096
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <event2/event.h>

#include "engine.h"
#include "eval.h"
#include "logging.h"

#define THINKER_POOL_COUNT 10

/**
 * @breif Handles errors in a thinker thread.
 *
 * If a thinker encounters an error, it must set the atomic termination flag,
 * log the information to the console, and exit. This function has the added
 * requirement that it must release the mutex before exiting.
 *
 * @param eng A printf compliant format string.
 * @param fmt A printf compliant format string.
 * @param ... All remaining arguments passed into the format string.
 *
 * @return This funciton does not reaturn.
 */
void thinker_unlock_write_err(engine_t *eng, char *fmt, ...)
{
    va_list args;
    atomic_store(&eng->exit_flag, true);
    pthread_mutex_unlock(&eng->sync_mtx);
    va_start(args, fmt);
    cibyl_vwrite_log(fmt, args);
    va_end(args);
    pthread_exit((void *)1);
}

/**
 * @breif Handles errors in a thinker thread.
 *
 * If a thinker encounters an error, it must set the atomic termination flag,
 * log the information to the console, and exit.
 *
 * @param eng The engine that the thinker belongs to.
 * @param fmt A printf compliant format string.
 * @param ... All remaining arguments passed into the format string.
 *
 * @return This funciton does not reaturn.
 */
void thinker_write_err(engine_t *eng, char *fmt, ...)
{
    va_list args;
    atomic_store(&eng->exit_flag, true);
    va_start(args, fmt);
    cibyl_vwrite_log(fmt, args);
    va_end(args);
    pthread_exit((void *)1);
}

/**
 * @breif Handles errors in a thinker thread.
 *
 * If a thinker encounters an error, it must set the atomic termination flag,
 * log the information to the console, and exit.
 *
 * @param eng The engine that the thinker belongs to.
 * @param prefix The prefix string (followed by ": ")
 * @param err_code The error code.
 */
void thinker_unlock_perror(engine_t *eng, char *prefix, int err_code)
{
    atomic_store(&eng->exit_flag, true);
    pthread_mutex_unlock(&eng->sync_mtx);
    cibyl_perror(prefix, err_code);
    pthread_exit((void *)1);
}

/**
 * @breif Handles errors in a thinker thread.
 *
 * If a thinker encounters an error, it must set the atomic termination flag,
 * log the information to the console, and exit.
 *
 * @param eng The engine that the thinker belongs to.
 * @param prefix The prefix string (followed by ": ")
 * @param err_code The error code.
 */
void thinker_perror(engine_t *eng, char *prefix, int err_code)
{
    atomic_store(&eng->exit_flag, true);
    cibyl_perror(prefix, err_code);
    pthread_exit((void *)1);
}

void *thinker_entry(void *eng_addr)
{
    int result;
    engine_t *eng = (engine_t *)eng_addr;
    ttable_t *ttable = &eng->ttable;
    cb_board_t board;
    cb_error_t err;

    while (true) {
        if ((result = pthread_mutex_lock(&eng->sync_mtx)) != 0)
            thinker_perror(eng, "thinker: pthread_mutex_lock", result);

        /* Synchonize on the condition and wait for a signal from the manager thread. */
        eng->waiting_threads++;
        while (!eng->go_rdy || atomic_load(&eng->exit_flag)) {
            if ((result = pthread_cond_wait(&eng->cnd_go_rdy, &eng->sync_mtx)) != 0)
                thinker_perror(eng, "thinker: pthread_cond_wait", result);
        }
        eng->waiting_threads--;

        /* Exit if we have encountered the exit flag. */
        if (atomic_load(&eng->exit_flag))
            break;
        
        /* Release the lock. */
        if ((result = pthread_mutex_unlock(&eng->sync_mtx)) != 0)
            thinker_perror(eng, "thinker: pthread_mutex_lock", result);

        /* Copy the board into the thinker. */
        board = eng->board;

        /* Start searching. */
        alphabeta(&board, eng->srch_params.depth);
    }

    return (void *)KH_EOK;
}

int eng_notify_go(engine_t *eng, const go_params_t *opts)
{
    int result;
    int retval;

    /* Aquire the lock on the engine. */
    if ((result = pthread_mutex_lock(&eng->sync_mtx)) != 0) {
        cibyl_perror("handler: pthread_mutex_lock", result);
        retval = result;
        goto out;
    }

    /* Wait for all thinkers to be ready. */
    while (!eng->go_rdy || atomic_load(&eng->exit_flag)) {
        if ((result = pthread_cond_wait(&eng->cnd_thrds_rdy, &eng->sync_mtx)) != 0) {
            cibyl_perror("handler: pthread_cond_wait", result);
            retval = result;
            goto out_release_lock;
        }
    }

    /* Broadcast search request to all thinkers. */
    eng->srch_params = *opts;
    eng->go_rdy = true;
    if ((result = pthread_cond_broadcast(&eng->cnd_go_rdy)) != 0) {
        cibyl_perror("handler: pthread_cond_broadcast", result);
        retval = result;
    }

out_release_lock:
    if ((result = pthread_mutex_unlock(&eng->sync_mtx)) != 0) {
        cibyl_perror("handler: pthread_mutex_unlock", result);
        retval = retval ? retval : result;
    }

out:
    return retval;
}

int eng_init(engine_t *eng, struct event *ev_done)
{
    int64_t result;
    int64_t retval = 0;
    int i;
    cb_error_t err;

    /* Initialize the board tables. */
    if ((result = cb_tables_init(&err)) != 0)
        cibyl_write_log("%s\n", err.desc);

    /* Spawn all of the thinkers. */
    for (i = 0; i < THINKER_POOL_COUNT; i++) {
        if ((result = pthread_create(&eng->mgr, NULL, thinker_entry, (void *)eng)) != 0) {
            retval = result;
            cibyl_perror("manager: pthread_join", result);
            goto err_close_thrds;
        }
    }

    /* Create the different events used by the engine. */
    eng->ev_done = ev_done;
    goto out;

err_close_thrds:
    /* Close the thinker threads. */
    for (; i > 0; i--) {
        result = pthread_join(eng->thinkers[i], NULL);

        /* Only log the first error we encounter. */
        if (result & !retval) {
            retval = result;
            cibyl_perror("manager: pthread_join", result);
        }
    }

    /* Cleanup tables on error. */
    cb_tables_free();
out:
    return result;
}

int eng_cleanup(engine_t *eng)
{

}
