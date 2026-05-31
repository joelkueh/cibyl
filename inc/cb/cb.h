
#ifndef CBLIB_H
#define CBLIB_H

#include <stdbool.h>
#include "cb/types.h"
#include "log.h"

/**
 * @breif Initializes a board. Note that this does not include move generation initalization.
 * @param err A pointer that will be populated with any errors.
 * @param board A pointer to the board to be initialized.
 * @return The error code corresponding to the error in err.
 */
cibyl_errno_t cb_board_init(cibyl_error_t *err, cb_board_t *board);

/**
 * @breif Initializes the move generation tables for a board.
 * @param err A pointer that will be populated with any errors.
 * @return True if this thread initialized the table, false otherwise.
 */
cibyl_errno_t cb_tables_init(cibyl_error_t *err);

/**
 * @breif Frees a board. Note that this does not clean up move generation tables.
 * @param board A pointer to the board to be freed.
 */
void cb_board_free(cb_board_t *board);

/**
 * @breif Frees the board tables. Not protected by a call_once lock.
 */
void cb_tables_free();

/**
 * @breif Populates a board from a fen string representation of a position.
 * @param err A pointer that will be populated with any errors.
 * @param board A pointer to the board to be populated.
 * @param fen The fen string to be parsed. This string will be modified by the function.
 * @return The error code corresponding to the error in err.
 */
cibyl_errno_t cb_board_from_fen(cibyl_error_t *err, cb_board_t *board, char *fen);

/**
 * @breif Populates a board from a fen string representation of a position.
 * @param err A pointer that will be populated with any errors.
 * @param board A pointer to the board to be populated.
 * @param uci The uci fen string to be parsed. This string will be modified by the function.
 * @return The error code corresponding to the error in err.
 */
cibyl_errno_t cb_board_from_uci(cibyl_error_t *err, cb_board_t *board, char *uci);

/**
 * @breif Populates a board from a fen string representation of a position.
 * @param err A pointer that will be populated with any errors.
 * @param board A pointer to the board to be populated.
 * @param pgn The pgn fen string to be parsed. This string will be modified by the function.
 * @return The error code corresponding to the error in err.
 */
cibyl_errno_t cb_board_from_pgn(cibyl_error_t *err, cb_board_t *board, char *pgn);

/**
 * @brief Copies posiiton information from board src into board dest.
 * @param err A pointer that will be populated with errors.
 * @param dest A pointer to the destination board.
 * @param src A pointer to the source board.
 * @return The error code corresponding to the error in err.
 */
cibyl_errno_t cb_board_from_copy(cibyl_error_t *err, cb_board_t *dest, cb_board_t *src);

/**
 * @breif Generates a move from a short algebraic string representation.
 *
 * Only generates moves that are valid for the particular position.
 * 
 * @param err A pointer that will be populated with any errors.
 * @param mv A cb_move_t struct that will be populated with the move if valid.
 * @param board A pointer to the board in question.
 * @param algbr The algebraic-notation string representation of the move.
 * @return The error code corresponding to the error in err.
 */
cibyl_errno_t cb_mv_from_short_algbr(cibyl_error_t *err, cb_move_t *mv, cb_board_t *board,
                                  const char *algbr);

/**
 * @breif Generates a move from a full uci algebraic string representation.
 *
 * Only generates moves that are valid for the particular position.
 * 
 * @param err A pointer that will be populated with any errors.
 * @param mv A cb_move_t struct that will be populated with the move if valid.
 * @param board A pointer to the board in question.
 * @param algbr The algebraic-notation string representation of the move.
 * @return The error code corresponding to the error in err.
 */
cibyl_errno_t cb_mv_from_uci_algbr(cibyl_error_t *err, cb_move_t *mv, cb_board_t *board,
                                const char *algbr);

/**
 * @breif Generates a valid uci algebraic string for a move.
 * @param buf String to be populated with the algebraic representation of the move.
 * @param mv The cb_move_t struct to be parsed.
 */
void cb_mv_to_uci_algbr(char buf[6], cb_move_t mv);

/**
 * @breif Generates the relevant board tables for a board state.
 * @param state The state table structure to populate.
 * @param board The board in question.
 */
void cb_gen_board_tables(cb_state_tables_t *state, cb_board_t *board);

/**
 * @breif Generates moves.
 * @param mvlst The movelist structure to populate.
 * @param board The board to generate moves on.
 * @param state The state table to reference for move generation.
 */
void cb_gen_moves(cb_mvlst_t *mvlst, cb_board_t *board, cb_state_tables_t *state);

/**
 * @breif Reserves space on the history stack to make at least added_depth moves.
 * @param err A pointer that will be populated with any errors.
 * @param board A pointer to the board in question.
 * @param added_depth The number of moves that must be made on top of the current position.
 * @return The error code corresponding to the error in err.
 */
cibyl_errno_t cb_reserve_for_make(cibyl_error_t *err, cb_board_t *board, uint32_t added_depth);

/**
 * @breif Makes a move on a board.
 *
 * This function applies a valid move onto a provided board. The caller of this function must
 * guarantee that there is adequate space on the history stack to make this move by calling
 * cb_reserve_for_make before calling this function.
 *
 * @param board The board to apply the move on.
 * @param mv The move to make.
 */
void cb_make(cb_board_t *board, const cb_move_t mv);

/**
 * @breif Unmakes a move on a board
 *
 * This function takes the last move in the board's history stack and undoes it. Note that it will
 * never cause a shrinkage of the history stack so future calls to cb_reserve_for_make are not
 * needed after a call to unmake.
 *
 * @param board The board to apply the move on.
 * @param mv The move to make.
 */
void cb_unmake(cb_board_t *board);

#endif /* CBLIB_H */
