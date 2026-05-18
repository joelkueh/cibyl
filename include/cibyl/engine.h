
#ifndef CYBIL_ENGINE_H
#define CYBIL_ENGINE_H

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>

#ifdef _WIN32
#include <namedpipeapi.h>
#endif

#include "cb_move.h"
#include "cibyl.h"

#define COMMAND_QUEUE_SIZE 32

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
    bool ready;             /**< Flag that states if the search is ready. */
} go_param_t;

/**
 * @breif Defines a Lazy SMP pool of threads that funcitons as an engine.
 */
struct engine {
    /* Fields for managing the board state. */
    cb_board_t board;       /**< The current position. */
    ttable_t ttable;        /**< The transposition tables. */
    go_param_t go_params;   /**< The parameters for any active search. */

    /* Synchronization primitives for the thread pool. */
    int nthinkers;          /**< The number of thinkers in the engine. */
    thinker_t manager;      /**< Backing data for the manager. */
    pthread_barrier_t init; /**< Initialization barrier. */
    pthread_barrier_t start;/**< Barrier for start of a search. */
    pthread_barrier_t end;  /**< Barrier for completion of a search. */
    atomic_bool stop_flag;  /**< Checked by the thinkers to see if they should stop search. */
    atomic_bool exit_flag;  /**< Checked by the thinkers to see if they should shut down. */
    
    /* Function pointers for data reporting. */
    void *udata;                        /**< User data for report handlers. */
    int (*report_error)(engine_t *eng); /**< Engine panic report function. */
    int (*report_best)(engine_t *eng);  /**< Engine bestmove report function. */
    int (*report_info)(engine_t *eng);  /**< Engine info report function. */
};

/**
 * @breif Clears a go_params struct.
 * @param params The struct to clear.
 */
static void clear_go_params(go_param_t *params) {
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
 * @return An error code for any failed threading calls.
 */
cibyl_errno_t eng_begin_init(engine_t *eng);

/**
 * @brief Blocks the calling thread until the engine is ready.
 * @param eng The engine to initialize.
 * @param report_fn The report function to register.
 */
void eng_await_ready(engine_t *eng);

/**
 * @brief Registers an error reporting function for the engine.
 * @param eng The engine to update.
 * @param report_fn The report function to register.
 */
void eng_register_error(engine_t *eng, int (*report_fn)(engine_t *eng));

/**
 * @brief Registers a bestmove reporting function for the engine.
 * @param eng The engine to update.
 * @param report_fn The report function to register.
 */
void eng_register_best(engine_t *eng, int (*report_fn)(engine_t *eng));

/**
 * @brief Registers an info reporting function for the engine.
 * @param eng The engine to update.
 * @param report_fn The report function to register.
 */
void eng_register_info(engine_t *eng, int (*report_fn)(engine_t *eng));

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
cibyl_errno_t eng_set_ucifen(engine_t *eng, char *fen);

/**
 * @breif Notifies the engine that it should begin a search.
 * @param eng The engine to notify.
 * @param opts The options for the search.
 */
void eng_notify_go(engine_t *eng, const go_param_t *opts);

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
