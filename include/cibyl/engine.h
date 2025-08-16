
#ifndef CYBIL_ENGINE_H
#define CYBIL_ENGINE_H

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <event2/event.h>

#ifdef _WIN32
#include <namedpipeapi.h>
#endif

#include "cblib/move.h"
#include "cblib/cblib.h"

/**
 * @breif A bag of parameters for an engine search.
 */
typedef struct {
    cb_mvlst_t searchmoves; /**< A list of moves to search from the root position. */
    int64_t wtime;          /**< The amount of time that white has left (msec). */
    int64_t btime;          /**< The amount of time that black has left (msec). */
    int64_t winc;           /**< The time increment for white (msec). */
    int64_t binc;           /**< The time increment for black (msec). */
    int64_t movestogo;      /**< The number of moves until the next time control. */
    int64_t depth;          /**< The depth of the search. */
    int64_t nodes;          /**< The number of nodes to search. */
    int64_t mate;           /**< Signifies this should be a mate search in mate moves. */
    int64_t movetime;       /**< The amount of time that the bot should think for. */
    bool ponder;            /**< Flag that states if this is a ponder search. */
    bool infinite;          /**< Flag that states if this search should be infinite. */
} go_params_t;

/**
 * @breif Transposition table that contains precalculated positions.
 */
typedef struct {

} ttable_t;

/**
 * @breif Defines struct for a thread that thinks.
 */
typedef struct {
    cb_board_t board;       /**< The board position used when thinking. */
    ttable_t *ttable;       /**< The transposition table to add thoughts to. */
    pthread_t thread;       /**< The thread itself. */
} thinker_t;

/**
 * @breif Defines a pool of threads that funcitons as an engine.
 */
typedef struct {
    /* Necessary board information. */
    cb_board_t board;           /**< The current position. */
    ttable_t ttable;            /**< The transposition tables. */
    pthread_t *thinkers;        /**< The pool of thinkers. */
    pthread_t mgr;              /**< The manager thread. */
    bool go_rdy;                /**< True if thinkers should begin searching. */

    /* Synchronization utiliteis. */
    pthread_cond_t cnd_thrds_rdy;   /**< Condition variable that handles thinker sync. */
    pthread_cond_t cnd_go_rdy;      /**< Condition variable that handles 'go' broadcast. */
    int waiting_threads;            /**< Holds the number of threads that are ready to run. */
    pthread_mutex_t sync_mtx;       /**< Mutex that handles thinker sync. */

    atomic_bool ponder;         /**< Monitored to see if pondering search. */
    atomic_bool exit_flag;      /**< Monitored to see if shutting down. */

    /* Information about the current search. */
    go_params_t srch_params;    /**< Parameters for the current search. */
    atomic_bool search_cncld;   /**< Bool for if search is canceled. */
    atomic_uint_fast64_t nodes; /**< Number of searched nodes. */
    atomic_uint_fast16_t depth; /**< Searched depth. */

    struct event *ev_done;      /**< Raised by a thinker when the search is done. */
    cb_move_t best_mv;          /**< Contains the best move that was pondered. */
    cb_move_t ponder_mv;        /**< Contains the move that was pondered. */
} engine_t;

/**
 * @breif Clears a go_params struct.
 * @param params The struct to clear.
 */
static void clear_go_params(go_params_t *params) {
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
 * @brief Begins initializing the engine.
 * @param eng The engine to initialize.
 * @param ev_done An event that the engine can set when it is done thinking.
 * @return An error code for any failed threading calls.
 */
int eng_init(engine_t *eng, struct event *ev_done);

/**
 * @breif Waits for engine initialization to be completed.
 * @param engine The engine that was to be initialized.
 */
int eng_await_isready(engine_t *eng);

/**
 * @breif Nicely frees all allocated memory and terminates threads.
 * @param engine The engine to cleanup.
 * @return An error code for any failed threading calls.
 */
int eng_cleanup(engine_t *eng);

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
int eng_set_ucifen(engine_t *eng, char *fen);

/**
 * @breif Notifies the engine that it should begin a search.
 * @param eng The engine to notify.
 * @param opts The options for the search.
 * @return An error code for any errors in notification.
 */
int eng_notify_go(engine_t *eng, const go_params_t *opts);

/**
 * @breif Notifies the engine that it should stop a search as soon as possible.
 *
 * Upon completing the search, the engine will send the bestmove and pondermove
 * out from its message pipe.
 *
 * @param eng The engine to notify.
 */
void eng_notify_stop(engine_t *eng);

/**
 * @breif Notifies the engine that the pondered move was played.
 *
 * The engine should switch from a ponder search to a normal search.
 *
 * @param eng THe engine to notify.
 */
void eng_notify_ponderhit(engine_t *eng);

#endif /* CIBYL_ENGINE_H */
