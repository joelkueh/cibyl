
#ifndef CYBIL_ENGINE_H
#define CYBIL_ENGINE_H

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>

#ifdef _WIN32
#include <namedpipeapi.h>
#endif

#include "cb/move.h"
#include "log.h"

#define DEFAULT_POOL_SIZE 1

/* Forward declaration of the engine struct. */
typedef struct engine engine_t;

/**
 * @breif Transposition table that contains precalculated positions.
 */
typedef struct {
} ttable_t;

/**
 * @breif Defines struct for a thread that thinks.
 */
typedef struct {
    cb_board_t *root;       /**< The root position to think on. */
    ttable_t *ttable;       /**< The transposition table to add thoughts to. */
    engine_t *eng;          /**< The engine that the thinker belongs to. */
    pthread_t thread;       /**< The thread that backs this thinker. */
    cb_board_t board;       /**< The board that the engine thinks on. */
    int tid;                /**< ID of the thinker thread. */
} thinker_t;

/**
 * @breif A bag of parameters for an engine search.
 */
typedef struct {
    cb_mvlst_t searchmoves; /**< A list of moves to search from the root position. */
    int64_t wtime;          /**< The amount of time that white has left. */
    int64_t btime;          /**< The amount of time that black has left. */
    int64_t winc;           /**< The time increment for white. */
    int64_t binc;           /**< The time increment for black. */
    int64_t movestogo;      /**< The number of moves to search. */
    int64_t depth;          /**< The depth of the search. */
    int64_t nodes;          /**< The number of nodes to search. */
    int64_t mate;           /**< Signifies this should be a mate search in mate moves. */
    int64_t movetime;       /**< The amount of time that the bot should think for. */
    bool ponder;            /**< Flag that states if this is a ponder search. */
    bool infinite;          /**< Flag that states if this search should be infinite. */
} search_params_t;

/**
 * @breif Defines a Lazy SMP pool of threads that funcitons as an engine.
 */
struct engine {
    /* Fields for managing the board state. */
    cb_board_t *board;      /**< The current position. */
    ttable_t *ttable;       /**< The transposition tables. */
    search_params_t params; /**< The parameters for any active search. */

    /* Synchronization primitives for the thread pool. */
    int nthinkers;             /**< The number of thinkers in the engine. */
    thinker_t *thinkers;       /**< Backing data for the manager. */
    pthread_mutex_t sync_lock; /**< Lock to help synchronize the engine. */
    pthread_cond_t signal;     /**< Condition that is signaled by the main thread. */
    pthread_cond_t ready;      /**< Condition for when the thread pool is ready to search. */
    int waiting_threads;       /**< Number of threads waiting on the lock. */
    atomic_int active_threads; /**< Number of threads currently searching. */
    atomic_bool search_flag;   /**< Checked by thinkers to see if they should search. */
    atomic_bool exit_flag;     /**< Checked by the thinkers to see if they should shut down. */
    
    /* Function pointers for data reporting. */
    cb_move_t bestmove; /**< TODO: Remove. */

    /* Error reporting pipe. */
#ifdef _WIN32
    /* TODO: Implement for windows. */
#else
    int error_pipe[2]; /**< Pipe for the thread pool to report errors on. */
#endif
};

/**
 * @breif Clears a go_params struct.
 * @param params The struct to clear.
 */
static void clear_search_params(search_params_t *params) {
    cb_mvlst_clear(&params->searchmoves);
    params->ponder = false;
    params->wtime = -1;
    params->btime = -1;
    params->winc = -1;
    params->binc = -1;
    params->movestogo = -1;
    params->depth = -1;
    params->nodes = -1;
    params->mate = -1;
    params->movetime = -1;
    params->infinite = true;
}

/**
 * @brief Initializes the engine struct.
 *
 * This initialization is minimal as per the UCI specification.
 * For complete initialization, see eng_preapre.
 *
 * @param eng The engine to initialize.
 */
void eng_init(engine_t *eng);

/**
 * @brief Initializes any uninitialized components (i.e. thread pool)
 *
 * On failure, this may leave an engine half-initialized.
 * This function guarantees, however, that a second call to eng_preapre
 * is safe, and a call to eng_deinit performs accurate cleanup after error.
 *
 * @param eng The engine to prepare.
 * @return An error code for any failed calls.
 */
cibyl_errno_t eng_prepare(cibyl_error_t *err, engine_t *eng);

/**
 * @brief Deinitializes the engine struct.
 *
 * Performs cleanup associated with the board, tables, and thread pool.
 *
 * @param eng The engine to deinitialize.
 */
void eng_deinit(engine_t *eng);

/**
 * @breif Nicely frees all allocated memory and terminates threads.
 * @param engine The engine to cleanup.
 */
void eng_cleanup(engine_t *eng);

/**
 * @breif Tells the engine that it is now playing a new game.
 * Dumps certain game-specific lookup tables.
 * @param engine The engine in question.
 */
void eng_newgame(engine_t *eng);

/**
 * @brief Initializes the position of the engine to a specific UCI fen string.
 * @param engine The engine in question.
 * @return An error code for any errors in initialization.
 */
cibyl_errno_t eng_set_ucifen(cibyl_error_t *err, engine_t *eng, char *fen);

/**
 * @breif Notifies the engine that it should begin a search.
 * @param eng The engine to notify.
 * @param opts The options for the search.
 */
cibyl_errno_t eng_start_search(cibyl_error_t *err, engine_t *eng, const search_params_t *opts);

/**
 * @brief Signals that all threads should terminate.
 * @param eng The engine to signal.
 */
void eng_broadcast_exit(engine_t *eng);

/**
 * @breif Notifies the engine that it should stop a search as soon as possible.
 *
 * Upon completing the search, the engine will send the bestmove and pondermove
 * out from its message pipe.
 *
 * @param eng The engine to notify.
 */
void eng_broadcast_stop(engine_t *eng);

/**
 * @breif Notifies the engine that the pondered move was played.
 *
 * The engine should switch from a ponder search to a normal search.
 *
 * @param eng THe engine to notify.
 */
void eng_notify_ponderhit(engine_t *eng);

#endif /* CIBYL_ENGINE_H */
