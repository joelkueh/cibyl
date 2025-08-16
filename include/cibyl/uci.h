
#ifndef CIBYL_UCI_H
#define CIBYL_UCI_H

#include <threads.h>
#include "engine.h"

/**
 * @breif Defines the different commands in the UCI standard.
 */
typedef enum {
    UCI = 0,        /**< Tells the engine to use the UCI interface. */
    DEBUG,          /**< Tells the engine to use DEBUG mode (more INFO responses). */
    ISREADY,        /**< Used to ping the engine to see if it is still alive. */
    SETOPTION,      /**< Allows you to change internal engine parameters. */
    REGISTER,       /**< Username and password stuff */
    UCINEWGAME,     /**< Tells the engine that the next search will be from a different game. */
    POSITION,       /**< Set up the position defined by the following string. */
    GO,             /**< Start calculating on the current position. */
    STOP,           /**< Stop calculating as soon as possible. */
    PONDERHIT,      /**< The user has played the expected move. */
    QUIT,           /**< Quit the program as soon as possible. */

    /* These are not in the UCI standard but they allow for debugging. */
    DISPLAY,        /**< Displays the board in a nice ascii format. */
    EVAL            /**< Logs information about evaluation to the console. */
} uci_cmd_t;

/**
 * @breif Defines the different responses in the UCI standard.
 */
typedef enum {
    ID = 0,         /**< Mandatory: Sent after UCI to identify the engine. */
    UCIOK,          /**< Mandatory: Sent after UCI. */
    READYOK,        /**< Mandatory: Always sent after isready. */
    BESTMOVE,       /**< Mandatory: Sent after a go command. Also may have ponder. */
    COPYPROTECTION, /**< Optional */
    REGISTRATION,   /**< Optional */
    INFO,           /**< Suggested: Read details. */
    OPTION          /**< Optional: Tells the GUI what internal params can be changed. */
} uci_rsp_t;

/**
 * @breif Defines a UCI chess engine.
 */
typedef struct {
    engine_t eng;       /**< The engine itself. */
    bool debug;         /**< Whether or not the engine is in debug mode. */
    go_params_t go_p;   /**< The parameters for the current 'go' serach. */

    struct event_base *ev_base;     /**< The event base for the UCI engine. */
    struct event *ev_timeout;       /**< Engine timeout event. */
    struct event *ev_done;          /**< Engine done event. */
    struct event *ev_usr_msg;       /**< User message event. */
} uci_engine_t;

/**
 * @breif Initializes a UCI chess engine. Performs absolute minimum work.
 * @param engine The engine to initialize.
 * @return 0 on success and -1 on error.
 */
int uci_init(uci_engine_t *engine);

#endif /* CIBYL_UCI_H */

