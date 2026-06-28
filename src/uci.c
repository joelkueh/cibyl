
#ifdef WIN32
#define strtok_r strtok_s
#endif
#include <string.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "cb/cb.h"
#include "cb/debug.h"
#include "uci.h"
#include "log.h"

const size_t MAX_COMMAND_LEN = 4096;

/* Command strings for matching. */
const char STR_UCI[] = "uci";
const char STR_DEBUG[] = "debug";
const char STR_ISREADY[] = "isready";
const char STR_SETOPTION[] = "setoption";
const char STR_REGISTER[] = "register";
const char STR_UCINEWGAME[] = "ucinewgame";
const char STR_POSITION[] = "position";
const char STR_GO[] = "go";
const char STR_STOP[] = "stop";
const char STR_PONDERHIT[] = "ponderhit";
const char STR_QUIT[] = "quit";
const char STR_UCI_DELIMS[] = " \t\n";
const char STR_FEN[] = "fen";

/* Supported option strings. */
const char STR_OPT_HASH[] = "Hash";
const char DECL_OPT_HASH[] = "option name Hash type spin default 16 min 1 max 32768\n";
const char STR_OPT_THREADS[] = "Threads";
const char DECL_OPT_THREADS[] = "option name Threads type spin default 1 min 1 max 16\n";
const char STR_OPT_CLEAR[] = "Clear Hash";
const char DECL_OPT_CLEAR[] = "option name Clear Hash type button";

/* Could suppor these later although it would be difficult. */
// const char STR_OPT_MULITPV[] = "MultiPV";
// const char STR_OPT_PONDER[] = "Ponder";
// const char STR_OPT_SYZYGY_PATH[] = "SyzygyPath";
// const char STR_OPT_SYZYGY_PROBE_DEPTH[] = "SyzygyProbeDepth";

/* Non-uci command strings. */
const char STR_DISPLAY[] = "d";
const char STR_MOVE[] = "m";
const char STR_UNDO[] = "u";
const char STR_DEBUG_PRINT[] = "dbg";
const char STR_EVAL[] = "eval";

/* Engine and author information. */
const char ENGINE_NAME[] = "Cibyl";
const char ENGINE_AUTHOR[] = "Joel Kuehne";

cibyl_errno_t uci_report_error(engine_t *eng, void *udata)
{
    /* TODO: Implement me. */
    printf("Error!\n");
    return CIBYL_EOK;
}

cibyl_errno_t uci_report_bestmove(engine_t *eng, void *udata)
{
    char buf[6];
    cb_mv_to_uci_algbr(buf, eng->bestmove);
    printf("bestmove %s\n", buf);
    return CIBYL_EOK;
}

cibyl_errno_t uci_report_info(engine_t *eng, void *udata)
{
    /* TODO: Implement me. */
    printf("Info!\n");
    return CIBYL_EOK;
}

cibyl_errno_t handle_position(cibyl_error_t *err, uci_engine_t *eng, char *opts)
{
    cibyl_errno_t result = CIBYL_EOK;
    char *saveptr;
    char *token;

    token = strtok_r(opts, STR_UCI_DELIMS, &saveptr);
    if (strcmp(token, STR_FEN) == 0) {
        if (eng_set_ucifen(err, &eng->eng, strtok_r(NULL, "\n", &saveptr))) {
            result = CIBYL_ERR_ADD_CONTEXT(err);
            goto out;
        }
    } else {
        result = CIBYL_MKERR(err, CIBYL_EINVAL, "invalid position format specifier: %s", token);
        goto out;
    }

out:
    return result;
}

cibyl_errno_t handle_move(cibyl_error_t *err, uci_engine_t *eng, char *token)
{
    cibyl_errno_t result = CIBYL_EOK;
    cb_move_t mv;

    /* Error handling. */
    if (token == NULL) {
        result = CIBYL_MKERR(err, CIBYL_EINVAL, "invalid move command");
        goto out;
    }

    /* Prepare the engine. */
    if (eng_prepare(err, &eng->eng)) {
        result = CIBYL_ERR_ADD_CONTEXT(err);
        goto out;
    }

    /* Parse the token into a move. */
    if (cb_mv_from_uci_algbr(err, &mv, eng->eng.board, token)) {
        result = CIBYL_ERR_ADD_CONTEXT(err);
        goto out;
    }

    /* Reserve space for the move. */
    if (cb_reserve_for_make(err, eng->eng.board, 1)) {
        result = CIBYL_ERR_ADD_CONTEXT(err);
        goto out;
    }
    cb_make(eng->eng.board, mv);

out:
    return result;
}

cibyl_errno_t handle_undo(cibyl_error_t *err, uci_engine_t *eng)
{
    cibyl_errno_t result = CIBYL_EOK;

    /* Prepare the engine. */
    if (eng_prepare(err, &eng->eng)) {
        result = CIBYL_ERR_ADD_CONTEXT(err);
        goto out;
    }

    /* Only unmake if there is a move to unmake. */
    if (eng->eng.board->hist.count <= 0) {
        result = CIBYL_MKERR(err, CIBYL_EINVAL, "cannot call unmake with empty history stack");
        goto out;
    }
    cb_unmake(eng->eng.board);

out:
    return result;
}

cibyl_errno_t handle_debug_print(cibyl_error_t *err, uci_engine_t *eng, char *token)
{
    cibyl_errno_t result = CIBYL_EOK;
    cb_mvlst_t mvlst;

    /* Error handling. */
    if (token == NULL) {
        result = CIBYL_MKERR(err, CIBYL_EINVAL, "invalid debug command");
        goto out;
    }

    /* Prepare the engine. */
    if (eng_prepare(err, &eng->eng)) {
        result = CIBYL_ERR_ADD_CONTEXT(err);
        goto out;
    }

    /* Parse the first token in the command. */
    cb_gen_moves(&mvlst, eng->eng.board);
    if (strcmp(token, "state") == 0)
        cb_print_state(stdout, eng->eng.board);
    else if (strcmp(token, "board") == 0)
        cb_print_piece_bitboards(stdout, eng->eng.board);
    else if (strcmp(token, "moves") == 0)
        cb_print_moves(stdout, &mvlst);
    else 
        result = CIBYL_MKERR(err, CIBYL_EINVAL, "invalid debug command: %s", token);
    
out:
    return result;
}

cibyl_errno_t handle_searchmoves(cibyl_error_t *err, char *moves)
{
    /* TODO: Implement me. */
    return CIBYL_EABORT;
}

cibyl_errno_t parse_i64(cibyl_error_t *err, char *token, int64_t *out)
{
    char *endp;

    if (token == NULL && *token == '\0') {
        return CIBYL_MKERR(err, CIBYL_EINVAL, "parse_i64: string expected");
    }
    *out = strtoll(token, &endp, 10);
    if (*endp != '\0') {
        return CIBYL_MKERR(err, CIBYL_EINVAL, "parse_i64: token is not an integer");
    }
    
    return CIBYL_EOK;
}

cibyl_errno_t handle_go(cibyl_error_t *err, uci_engine_t *eng, char *opts)
{
    typedef enum {
        GO_SEARCHMOVES,
        GO_PONDER,
        GO_WTIME,
        GO_BTIME,
        GO_WINC,
        GO_BINC,
        GO_MOVESTOGO,
        GO_DEPTH,
        GO_NODES,
        GO_MATE,
        GO_MOVETIME,
        GO_INFINITE
    } go_opt_t;

    const char STR_GO_SEARCHMOVES[] = "searchmoves";
    const char STR_GO_PONDER[] = "ponder";
    const char STR_GO_WTIME[] = "wtime";
    const char STR_GO_BTIME[] = "btime";
    const char STR_GO_WINC[] = "winc";
    const char STR_GO_BINC[] = "binc";
    const char STR_GO_MOVESTOGO[] = "movestogo";
    const char STR_GO_DEPTH[] = "depth";
    const char STR_GO_NODES[] = "nodes";
    const char STR_GO_MATE[] = "mate";
    const char STR_GO_MOVETIME[] = "movetime";
    const char STR_GO_INFINITE[] = "infinite";
    const char STR_GO_PERFT[] = "perft";

    cibyl_errno_t result = CIBYL_EOK;
    search_params_t search_params;
    char *saveptr;
    char *token;
    char *endp;
    
    /* Clear go params. */
    clear_search_params(&search_params);

    /* Handle null input. */
    if (opts == NULL) {
        search_params.infinite = true;
        goto search;
    }

    /* Loop through all of the arguments to the go command. */
    token = strtok_r(opts, STR_UCI_DELIMS, &saveptr);
    while (token != NULL) {
        /* Handle searchmoves, this will consume all remaining arguments. */
        if (strcmp(token, STR_GO_SEARCHMOVES) == 0) {
            if (handle_searchmoves(err, strtok_r(NULL, STR_UCI_DELIMS, &saveptr))) {
                result = CIBYL_ERR_ADD_CONTEXT(err);
                goto out;
            }
            goto search;
        }

        /* Handle various flags. */
        else if (strcmp(token, STR_GO_PONDER) == 0) {
            search_params.ponder = true;
        } else if (strcmp(token, STR_GO_INFINITE) == 0) {
            search_params.infinite = true;
        } else if (strcmp(token, STR_GO_PERFT) == 0) {
            search_params.perft = true;
            if (parse_i64(err, strtok_r(NULL, STR_UCI_DELIMS, &saveptr), &search_params.depth)) {
                result = CIBYL_ERR_ADD_CONTEXT(err);
                goto out;
            }
        }

        /* Handle time information. */
        else if (strcmp(token, STR_GO_WTIME) == 0) {
            if (parse_i64(err, strtok_r(NULL, STR_UCI_DELIMS, &saveptr), &search_params.wtime)) {
                result = CIBYL_ERR_ADD_CONTEXT(err);
                goto out;
            }
        } else if (strcmp(token, STR_GO_BTIME) == 0) {
            if (parse_i64(err, strtok_r(NULL, STR_UCI_DELIMS, &saveptr), &search_params.btime)) {
                result = CIBYL_ERR_ADD_CONTEXT(err);
                goto out;
            }
        } else if (strcmp(token, STR_GO_WINC) == 0) {
            if (parse_i64(err, strtok_r(NULL, STR_UCI_DELIMS, &saveptr), &search_params.winc)) {
                result = CIBYL_ERR_ADD_CONTEXT(err);
                goto out;
            }
        } else if (strcmp(token, STR_GO_BINC) == 0) {
            if (parse_i64(err, strtok_r(NULL, STR_UCI_DELIMS, &saveptr), &search_params.binc)) {
                result = CIBYL_ERR_ADD_CONTEXT(err);
                goto out;
            }
        }

        /* Handle move stopping. */
        else if (strcmp(token, STR_GO_MOVESTOGO) == 0) {
            if (parse_i64(err, strtok_r(NULL, STR_UCI_DELIMS, &saveptr), &search_params.movestogo)) {
                result = CIBYL_ERR_ADD_CONTEXT(err);
                goto out;
            }
        } else if (strcmp(token, STR_GO_DEPTH) == 0) {
            if (parse_i64(err, strtok_r(NULL, STR_UCI_DELIMS, &saveptr), &search_params.depth)) {
                result = CIBYL_ERR_ADD_CONTEXT(err);
                goto out;
            }
        } else if (strcmp(token, STR_GO_NODES) == 0) {
            if (parse_i64(err, strtok_r(NULL, STR_UCI_DELIMS, &saveptr), &search_params.nodes)) {
                result = CIBYL_ERR_ADD_CONTEXT(err);
                goto out;
            }
        } else if (strcmp(token, STR_GO_MATE) == 0) {
            if (parse_i64(err, strtok_r(NULL, STR_UCI_DELIMS, &saveptr), &search_params.mate)) {
                result = CIBYL_ERR_ADD_CONTEXT(err);
                goto out;
            }
        } else if (strcmp(token, STR_GO_MOVETIME) == 0) {
            if (parse_i64(err, strtok_r(NULL, STR_UCI_DELIMS, &saveptr), &search_params.movetime)) {
                result = CIBYL_ERR_ADD_CONTEXT(err);
                goto out;
            }
        }

        /* Grab the next token. */
        token = strtok_r(NULL, STR_UCI_DELIMS, &saveptr);
    }

search:
    /* Start thinking. */
    if (eng_start_search(err, &eng->eng, &search_params)) {
        result = CIBYL_ERR_ADD_CONTEXT(err);
        goto out;
    }

out:
    return result;
}

cibyl_errno_t handle_cmd(cibyl_error_t *err, uci_engine_t *eng, char *cmd)
{
    cibyl_errno_t result = CIBYL_EOK;
    bool token_accepted = false;
    char *saveptr;
    char *token;
    char *opts;

    token = strtok_r(cmd, STR_UCI_DELIMS, &saveptr);
    while (!token_accepted && token != NULL) {
        token_accepted = true;

        /* Initialization commands. */
        if (strcmp(token, STR_UCI) == 0) {
            /* Respond to the UCI command. */
            printf("id name %s\n", ENGINE_NAME);
            printf("id author %s\n", ENGINE_AUTHOR);
            printf("uciok\n");
        } else if (strcmp(token, STR_DEBUG) == 0) {
            eng->debug = true;
        } else if (strcmp(token, STR_ISREADY) == 0) {
            /* Wait for the engine to be ready to process input. */
            if (eng_prepare(err, &eng->eng) != CIBYL_EOK) {
                CIBYL_ERR_ADD_CONTEXT(err);
                result = CIBYL_EABORT;
                goto err;
            }
            printf("readyok\n");
        } else if (strcmp(token, STR_SETOPTION) == 0) {
            /* TODO: Implement me. */
        } else if (strcmp(token, STR_REGISTER) == 0) {
            /* Do nothing as cibyl is not password protected. */
        }

        /* Board setup commands. */
        else if (strcmp(token, STR_UCINEWGAME) == 0) {
            if (eng_set_ucifen(err, &eng->eng, "startpos") != CIBYL_EOK) {
                CIBYL_ERR_ADD_CONTEXT(err);
                result = CIBYL_EABORT;
                goto err;
            }
        } else if (strcmp(token, STR_POSITION) == 0) {
            if (handle_position(err, eng, strtok_r(NULL, "\n", &saveptr)) != CIBYL_EOK) {
                CIBYL_ERR_ADD_CONTEXT(err);
                result = CIBYL_EABORT;
                goto err;
            }
        }

        /* Functions for controlling thinking. */
        else if (strcmp(token, STR_GO) == 0) {
            if (handle_go(err, eng, strtok_r(NULL, "", &saveptr)) != CIBYL_EOK) {
                CIBYL_ERR_ADD_CONTEXT(err);
                result = CIBYL_EABORT;
                goto err;
            }
        } else if (strcmp(token, STR_STOP) == 0) {
            eng_broadcast_stop(&eng->eng);
        } else if (strcmp(token, STR_PONDERHIT) == 0) {
            eng_notify_ponderhit(&eng->eng);
        }

        /* Miscelaneous functions. */
        else if (strcmp(token, STR_QUIT) == 0) {
            eng->exit = true;
        } else if (strcmp(token, STR_DISPLAY) == 0) {
            if (eng_prepare(err, &eng->eng)) {
                CIBYL_ERR_ADD_CONTEXT(err);
                result = CIBYL_EABORT;
                goto err;
            }
            cb_print_board_ascii(stdout, eng->eng.board);
        } else if (strcmp(token, STR_MOVE) == 0) {
            if (handle_move(err, eng, strtok_r(NULL, STR_UCI_DELIMS, &saveptr))) {
                CIBYL_ERR_ADD_CONTEXT(err);
                result = CIBYL_EABORT;
                goto err;
            }
        } else if (strcmp(token, STR_UNDO) == 0) {
            if (handle_undo(err, eng)) {
                CIBYL_ERR_ADD_CONTEXT(err);
                result = CIBYL_EABORT;
                goto err;
            }
        } else if (strcmp(token, STR_DEBUG_PRINT) == 0) {
            if (handle_debug_print(err, eng, strtok_r(NULL, STR_UCI_DELIMS, &saveptr))) {
                CIBYL_ERR_ADD_CONTEXT(err);
                result = CIBYL_EABORT;
                goto err;
            }
        }

        /* If the first word is invalid, then parse starting with the next as per the spec. */
        else {
            token = strtok_r(NULL, STR_UCI_DELIMS, &saveptr);
            token_accepted = false;
        }
    }

err:
    return result;
}

void uci_init(uci_engine_t *eng)
{
    /* Set the engine as uninitialized. */
    eng->debug = false;
    eng->initialized = false;
    eng->exit = false;

    /* Bring the underlying engine to a sane state. */
    eng_init(&eng->eng);
}

cibyl_errno_t uci_process(cibyl_error_t *err, uci_engine_t *eng)
{
    char cmd[MAX_COMMAND_LEN];

    /* TODO: Use select to wait for data from multiple fds.
     * This isn't a big deal until threads can throw errors (tablebase reads). */
    while (!eng->exit && fgets(cmd, MAX_COMMAND_LEN, stdin) != NULL) {
        if (handle_cmd(err, eng, cmd)) {
            CIBYL_ERR_ADD_CONTEXT(err);
            return CIBYL_EABORT;
        }
    }

    /* TODO: Detect if the exit was caused by an error in the engine. */
    return CIBYL_EOK;
}

void uci_deinit(uci_engine_t *eng)
{
    /* Deinitialize the engine. */
    eng_deinit(&eng->eng);
}
