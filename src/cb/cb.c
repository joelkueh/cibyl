
#ifdef WIN32
#define strtok_r strtok_s
#endif
#include <string.h>

#include <errno.h>
#include <threads.h>
#include <stdatomic.h>

#include "log.h"
#include "cb/cb.h"
#include "cb/tables.h"
#include "cb/const.h"
#include "cb/move.h"
#include "cb/boardrep.h"
#include "cb/history.h"

int cb_table_refcount = 0;

void cb_mv_to_uci_algbr(char *buf, cb_move_t move)
{
    buf[0] = cb_mv_get_from(move) % 8 + 'a';
    buf[1] = '1' + cb_mv_get_from(move) / 8;
    buf[2] = cb_mv_get_to(move) % 8 + 'a';
    buf[3] = '1' + cb_mv_get_to(move) / 8;

    switch (cb_mv_get_flags(move)) {
        case CB_MV_KNIGHT_PROMO:
        case CB_MV_KNIGHT_PROMO_CAPTURE:
            buf[4] = 'n';
            buf[5] = '\0';
            break;
        case CB_MV_BISHOP_PROMO:
        case CB_MV_BISHOP_PROMO_CAPTURE:
            buf[4] = 'b';
            buf[5] = '\0';
            break;
        case CB_MV_ROOK_PROMO:
        case CB_MV_ROOK_PROMO_CAPTURE:
            buf[4] = 'r';
            buf[5] = '\0';
            break;
        case CB_MV_QUEEN_PROMO:
        case CB_MV_QUEEN_PROMO_CAPTURE:
            buf[4] = 'q';
            buf[5] = '\0';
            break;
        default:
            buf[4] = '\0';
            break;
    }
}

cibyl_errno_t cb_board_init(cibyl_error_t *err, cb_board_t *board)
{
    cibyl_errno_t result = CIBYL_EOK;
    if ((result = cb_hist_stack_init(&board->hist)) != CIBYL_EOK)
        (void)CIBYL_MKERR(err, result, "malloc: %s\n", strerror(errno));
    return result;
}

cibyl_errno_t cb_tables_init(cibyl_error_t *err)
{
    cibyl_errno_t result = CIBYL_EOK;
    cb_init_normal_tables();
    if ((result = cb_init_magic_tables()) != CIBYL_EOK)
        (void)CIBYL_MKERR(err, result, "malloc: %s\n", strerror(errno));
    return result;
}

void cb_tables_free()
{
    cb_free_magic_tables();
}

void cb_board_free(cb_board_t *board)
{
    cb_hist_stack_free(&board->hist);
}

cibyl_errno_t cb_reserve_for_make(cibyl_error_t *err, cb_board_t *board, uint32_t added_depth)
{
    cibyl_errno_t result = CIBYL_EOK;
    if ((result = cb_hist_stack_reserve(&board->hist, added_depth)) != 0)
        (void)CIBYL_MKERR(err, result, "realloc: %s\n", strerror(errno));
    return result;
}

void cb_make(cb_board_t *board, const cb_move_t mv)
{
    cb_history_t old_state = board->hist.data[board->hist.count - 1].hist;
    cb_hist_ele_t new_ele;
    cb_mv_flag_t flag = cb_mv_get_flags(mv);
    uint8_t to = cb_mv_get_to(mv);
    uint8_t from = cb_mv_get_from(mv);

    cb_ptype_t ptype;
    cb_ptype_t cap_ptype;
    cb_history_t new_state = old_state;

    /* Variables for castles. */
    uint8_t rook_from;
    uint8_t rook_to;

    /* Variables for enp. */
    int8_t direction;

    /* Make the move. */
    switch (flag)
    {
        case CB_MV_QUIET:
            ptype = cb_ptype_at_sq(board, from);
            cb_hist_set_captured_piece(&new_state, CB_PTYPE_EMPTY);
            cb_hist_decay_castle_rights(&new_state, board->turn, to, from);
            cb_write_piece(board, to, ptype, board->turn);
            cb_delete_piece(board, from, ptype, board->turn);
            break;
        case CB_MV_CAPTURE:
            ptype = cb_ptype_at_sq(board, from);
            cap_ptype = cb_ptype_at_sq(board, to);
            cb_hist_set_captured_piece(&new_state, cap_ptype);
            cb_hist_decay_castle_rights(&new_state, board->turn, to, from);
            cb_replace_piece(board, to, ptype, board->turn, cap_ptype, !board->turn);
            cb_delete_piece(board, from, ptype, board->turn);
            break;
        case CB_MV_DOUBLE_PAWN_PUSH:
            cb_hist_set_enp(&new_state, to & 0b111);
            cb_write_piece(board, to, CB_PTYPE_PAWN, board->turn);
            cb_delete_piece(board, from, CB_PTYPE_PAWN, board->turn);
            break;
        case CB_MV_KING_SIDE_CASTLE:
            rook_from = board->turn ? M_WHITE_KING_SIDE_ROOK_START :
                M_BLACK_KING_SIDE_ROOK_START;
            rook_to = board->turn ? M_WHITE_KING_SIDE_ROOK_TARGET :
                M_BLACK_KING_SIDE_ROOK_TARGET;
            cb_hist_set_captured_piece(&new_state, CB_PTYPE_EMPTY);
            cb_hist_remove_castle(&new_state, board->turn);
            cb_delete_piece(board, from, CB_PTYPE_KING, board->turn);
            cb_write_piece(board, to, CB_PTYPE_KING, board->turn);
            cb_delete_piece(board, rook_from, CB_PTYPE_ROOK, board->turn);
            cb_write_piece(board, rook_to, CB_PTYPE_ROOK, board->turn);
            break;
        case CB_MV_QUEEN_SIDE_CASTLE:
            rook_from = board->turn ? M_WHITE_QUEEN_SIDE_ROOK_START :
                M_BLACK_QUEEN_SIDE_ROOK_START;
            rook_to = board->turn ? M_WHITE_QUEEN_SIDE_ROOK_TARGET :
                M_BLACK_QUEEN_SIDE_ROOK_TARGET;
            cb_hist_set_captured_piece(&new_state, CB_PTYPE_EMPTY);
            cb_hist_remove_castle(&new_state, board->turn);
            cb_delete_piece(board, from, CB_PTYPE_KING, board->turn);
            cb_write_piece(board, to, CB_PTYPE_KING, board->turn);
            cb_delete_piece(board, rook_from, CB_PTYPE_ROOK, board->turn);
            cb_write_piece(board, rook_to, CB_PTYPE_ROOK, board->turn);
            break;
        case CB_MV_ENPASSANT:
            direction = board->turn == CB_WHITE ? -8 : 8;
            cb_hist_set_captured_piece(&new_state, CB_PTYPE_PAWN);
            cb_write_piece(board, to, CB_PTYPE_PAWN, board->turn);
            cb_delete_piece(board, from, CB_PTYPE_PAWN, board->turn);
            cb_delete_piece(board, to + direction, CB_PTYPE_PAWN, !board->turn);
            break;
        case CB_MV_KNIGHT_PROMO:
            cb_hist_set_captured_piece(&new_state, CB_PTYPE_EMPTY);
            cb_write_piece(board, to, CB_PTYPE_KNIGHT, board->turn);
            cb_delete_piece(board, from, CB_PTYPE_PAWN, board->turn);
            break;
        case CB_MV_BISHOP_PROMO:
            cb_hist_set_captured_piece(&new_state, CB_PTYPE_EMPTY);
            cb_write_piece(board, to, CB_PTYPE_BISHOP, board->turn);
            cb_delete_piece(board, from, CB_PTYPE_PAWN, board->turn);
            break;
        case CB_MV_ROOK_PROMO:
            cb_hist_set_captured_piece(&new_state, CB_PTYPE_EMPTY);
            cb_write_piece(board, to, CB_PTYPE_ROOK, board->turn);
            cb_delete_piece(board, from, CB_PTYPE_PAWN, board->turn);
            break;
        case CB_MV_QUEEN_PROMO:
            cb_hist_set_captured_piece(&new_state, CB_PTYPE_EMPTY);
            cb_write_piece(board, to, CB_PTYPE_QUEEN, board->turn);
            cb_delete_piece(board, from, CB_PTYPE_PAWN, board->turn);
            break;
        case CB_MV_KNIGHT_PROMO_CAPTURE:
            cap_ptype = cb_ptype_at_sq(board, to);
            cb_hist_set_captured_piece(&new_state, cap_ptype);
            cb_hist_decay_castle_rights(&new_state, board->turn, to, from);
            cb_replace_piece(board, to, CB_PTYPE_KNIGHT, board->turn, cap_ptype, !board->turn);
            cb_delete_piece(board, from, CB_PTYPE_PAWN, board->turn);
            break;
        case CB_MV_BISHOP_PROMO_CAPTURE:
            cap_ptype = cb_ptype_at_sq(board, to);
            cb_hist_set_captured_piece(&new_state, cap_ptype);
            cb_hist_decay_castle_rights(&new_state, board->turn, to, from);
            cb_replace_piece(board, to, CB_PTYPE_BISHOP, board->turn, cap_ptype, !board->turn);
            cb_delete_piece(board, from, CB_PTYPE_PAWN, board->turn);
            break;
        case CB_MV_ROOK_PROMO_CAPTURE:
            cap_ptype = cb_ptype_at_sq(board, to);
            cb_hist_set_captured_piece(&new_state, cap_ptype);
            cb_hist_decay_castle_rights(&new_state, board->turn, to, from);
            cb_replace_piece(board, to, CB_PTYPE_ROOK, board->turn, cap_ptype, !board->turn);
            cb_delete_piece(board, from, CB_PTYPE_PAWN, board->turn);
            break;
        case CB_MV_QUEEN_PROMO_CAPTURE:
            cap_ptype = cb_ptype_at_sq(board, to);
            cb_hist_set_captured_piece(&new_state, cap_ptype);
            cb_hist_decay_castle_rights(&new_state, board->turn, to, from);
            cb_replace_piece(board, to, CB_PTYPE_QUEEN, board->turn, cap_ptype, !board->turn);
            cb_delete_piece(board, from, CB_PTYPE_PAWN, board->turn);
            break;
    }

    /* Save the new state to the stack. */
    board->turn = !board->turn;
    new_ele.hist = new_state;
    new_ele.move = mv;
    cb_hist_stack_push(&board->hist, new_ele);
}

void cb_unmake(cb_board_t *board)
{
    cb_hist_ele_t old_ele = cb_hist_stack_pop(&board->hist);
    cb_hist_ele_t new_ele;
    cb_mv_flag_t flag = cb_mv_get_flags(old_ele.move);
    uint8_t to = cb_mv_get_to(old_ele.move);
    uint8_t from = cb_mv_get_from(old_ele.move);

    cb_ptype_t ptype;
    cb_ptype_t cap_ptype;

    /* Variables for castles. */
    uint8_t rook_from;
    uint8_t rook_to;

    /* Variables for enp. */
    int8_t direction;

    /* Unmake the move. */
    board->turn = !board->turn;
    switch (flag) {
        case CB_MV_QUIET:
        case CB_MV_DOUBLE_PAWN_PUSH:
            ptype = cb_ptype_at_sq(board, to);
            cb_write_piece(board, from, ptype, board->turn);
            cb_delete_piece(board, to, ptype, board->turn);
            break;
        case CB_MV_CAPTURE:
            ptype = cb_ptype_at_sq(board, to);
            cap_ptype = cb_hist_get_captured_piece(&old_ele.hist);
            cb_write_piece(board, from, ptype, board->turn);
            cb_replace_piece(board, to, cap_ptype, !board->turn, ptype, board->turn);
            break;
        case CB_MV_KING_SIDE_CASTLE:
            rook_from = board->turn ? M_WHITE_KING_SIDE_ROOK_START :
                M_BLACK_KING_SIDE_ROOK_START;
            rook_to = board->turn ? M_WHITE_KING_SIDE_ROOK_TARGET :
                M_BLACK_KING_SIDE_ROOK_TARGET;
            cb_write_piece(board, from, CB_PTYPE_KING, board->turn);
            cb_delete_piece(board, to, CB_PTYPE_KING, board->turn);
            cb_write_piece(board, rook_from, CB_PTYPE_ROOK, board->turn);
            cb_delete_piece(board, rook_to, CB_PTYPE_ROOK, board->turn);
            break;
        case CB_MV_QUEEN_SIDE_CASTLE:
            rook_from = board->turn ? M_WHITE_QUEEN_SIDE_ROOK_START :
                M_BLACK_QUEEN_SIDE_ROOK_START;
            rook_to = board->turn ? M_WHITE_QUEEN_SIDE_ROOK_TARGET :
                M_BLACK_QUEEN_SIDE_ROOK_TARGET;
            cb_write_piece(board, from, CB_PTYPE_KING, board->turn);
            cb_delete_piece(board, to, CB_PTYPE_KING, board->turn);
            cb_write_piece(board, rook_from, CB_PTYPE_ROOK, board->turn);
            cb_delete_piece(board, rook_to, CB_PTYPE_ROOK, board->turn);
            break;
        case CB_MV_ENPASSANT:
            direction = board->turn == CB_WHITE ? -8 : 8;
            cb_write_piece(board, from, CB_PTYPE_PAWN, board->turn);
            cb_delete_piece(board, to, CB_PTYPE_PAWN, board->turn);
            cb_write_piece(board, to + direction, CB_PTYPE_PAWN, !board->turn);
            break;
        case CB_MV_KNIGHT_PROMO:
            cb_write_piece(board, from, CB_PTYPE_PAWN, board->turn);
            cb_delete_piece(board, to, CB_PTYPE_KNIGHT, board->turn);
            break;
        case CB_MV_BISHOP_PROMO:
            cb_write_piece(board, from, CB_PTYPE_PAWN, board->turn);
            cb_delete_piece(board, to, CB_PTYPE_BISHOP, board->turn);
            break;
        case CB_MV_ROOK_PROMO:
            cb_write_piece(board, from, CB_PTYPE_PAWN, board->turn);
            cb_delete_piece(board, to, CB_PTYPE_ROOK, board->turn);
            break;
        case CB_MV_QUEEN_PROMO:
            cb_write_piece(board, from, CB_PTYPE_PAWN, board->turn);
            cb_delete_piece(board, to, CB_PTYPE_QUEEN, board->turn);
            break;
        case CB_MV_KNIGHT_PROMO_CAPTURE:
            cap_ptype = cb_hist_get_captured_piece(&old_ele.hist);
            cb_write_piece(board, from, CB_PTYPE_PAWN, board->turn);
            cb_replace_piece(board, to, cap_ptype, !board->turn, CB_PTYPE_KNIGHT, board->turn);
            break;
        case CB_MV_BISHOP_PROMO_CAPTURE:
            cap_ptype = cb_hist_get_captured_piece(&old_ele.hist);
            cb_write_piece(board, from, CB_PTYPE_PAWN, board->turn);
            cb_replace_piece(board, to, cap_ptype, !board->turn, CB_PTYPE_BISHOP, board->turn);
            break;
        case CB_MV_ROOK_PROMO_CAPTURE:
            cap_ptype = cb_hist_get_captured_piece(&old_ele.hist);
            cb_write_piece(board, from, CB_PTYPE_PAWN, board->turn);
            cb_replace_piece(board, to, cap_ptype, !board->turn, CB_PTYPE_ROOK, board->turn);
            break;
        case CB_MV_QUEEN_PROMO_CAPTURE:
            cap_ptype = cb_hist_get_captured_piece(&old_ele.hist);
            cb_write_piece(board, from, CB_PTYPE_PAWN, board->turn);
            cb_replace_piece(board, to, cap_ptype, !board->turn, CB_PTYPE_QUEEN, board->turn);
            break;
    }
}

cibyl_errno_t cb_mv_from_short_algbr(cibyl_error_t *err, cb_move_t *mv, cb_board_t *board,
                                  const char *algbr)
{
    assert(false && "not yet implemented");
    return 0;
}

cibyl_errno_t cb_mv_from_uci_algbr(cibyl_error_t *err, cb_move_t *mv, cb_board_t *board,
                                const char *algbr)
{
    cibyl_errno_t result = CIBYL_EOK;
    uint8_t to, from;
    uint16_t flag;
    cb_move_t temp_mv;
    cb_mvlst_t mvlst;
    int i;

    /* Make sure that the length of the algebraic string is 4 or 5 characters. */
    if (strlen(algbr) != 4 && strlen(algbr) != 5) {
        result = CIBYL_MKERR(err, CIBYL_EINVAL, "invalid move string length");
        goto out;
    }

    /* Get the information about the move itself. */
    from = algbr[0] - 'a';
    from += (algbr[1] - '1') * 8;
    to = algbr[2] - 'a';
    to += (algbr[3] - '1') * 8;

    /* Verify that the characters inputted were valid. */
    if (from < 0 || from >= 64 || to < 0 || to >= 64) {
        result = CIBYL_MKERR(err, CIBYL_EINVAL, "invalid character in move");
        goto out;
    }

    /* Find the move in the list of generated moves and return the match. */
    cb_gen_moves(&mvlst, board);
    *mv = CB_INVALID_MOVE;
    for (i = 0; i < cb_mvlst_size(&mvlst); i++) {
        temp_mv = cb_mvlst_at(&mvlst, i);
        if (cb_mv_get_from(temp_mv) == from && cb_mv_get_to(temp_mv) == to) {
            *mv = temp_mv;
            break;
        }
    }

    /* Check if the move was not valid. */
    if (*mv == CB_INVALID_MOVE) {
        result = CIBYL_MKERR(err, CIBYL_EINVAL, "illegal move specified");
        goto out;
    }

    /* Adjust the move for information about promotions. */
    flag = cb_mv_get_flags(*mv);
    if (flag & (1 << 15)) { /* Check if this is a promotion. */
        flag &= ~(0b11 << 12); /* Remove the lower two bits from the flag. */

        /* Check what the piece type is for a promotion and add the offset to turn
         * the piece type for the capture into what we want. */
        switch (algbr[4]) {
            case 'n':
                flag += 0 << 12;
                break;
            case 'b':
                flag += 1 << 12;
                break;
            case 'r':
                flag += 2 << 12;
                break;
            case 'q':
                flag += 3 << 12;
                break;
            default:
                result = CIBYL_MKERR(err, CIBYL_EINVAL, "invlaid character in move");
                goto out;
        }

        /* Recreate the move. */
        *mv = cb_mv_from_data(from, to, flag);
    }

out:
    return result;
}

cibyl_errno_t parse_fen_main(cibyl_error_t *err, cb_board_t *board, char *fen_main)
{
    cibyl_errno_t result = CIBYL_EOK;
    uint8_t row = 7;
    uint8_t sq = row * 8;
    char c;
    int i = 0;
    cb_color_t pcolor;

    /* Error handling. */
    if (fen_main == NULL) {
        result = CIBYL_MKERR(err, CIBYL_EINVAL, "missing fen body");
        goto out;
    }

    /* Parse the main portion of the fen string. */
    while ((c = fen_main[i++]) != '\0') {
        if ('1' <= c && c <= '8') {
            sq += c - '0';
            /* Check if we should have had a '/' in our input string by now. */
            if (sq < row * 8 || sq > row * 8 + 8) {
                result = CIBYL_MKERR(err, CIBYL_EINVAL, "row overrun without encountering '/'");
                goto out;
            }
        } else if (c == 'p' || c == 'P') {
            cb_write_piece(board, sq, CB_PTYPE_PAWN, c < 'a' ? CB_WHITE : CB_BLACK);
            sq++;
        } else if (c == 'n' || c == 'N') {
            cb_write_piece(board, sq, CB_PTYPE_KNIGHT, c < 'a' ? CB_WHITE : CB_BLACK);
            sq++;
        } else if (c == 'b' || c == 'B') {
            cb_write_piece(board, sq, CB_PTYPE_BISHOP, c < 'a' ? CB_WHITE : CB_BLACK);
            sq++;
        } else if (c == 'r' || c == 'R') {
            cb_write_piece(board, sq, CB_PTYPE_ROOK, c < 'a' ? CB_WHITE : CB_BLACK);
            sq++;
        } else if (c == 'q' || c == 'Q') {
            cb_write_piece(board, sq, CB_PTYPE_QUEEN, c < 'a' ? CB_WHITE : CB_BLACK);
            sq++;
        } else if (c == 'k' || c == 'K') {
            cb_write_piece(board, sq, CB_PTYPE_KING, c < 'a' ? CB_WHITE : CB_BLACK);
            sq++;
        } else if (c == '/') {
            /* If we are not at the end of the row, we don't want to get a '/'. */
            if (sq % 8 != 0) {
                result = CIBYL_MKERR(err, CIBYL_EINVAL, "encountered '/' before the end of a row");
                goto out;
            }
            row -= 1;
            if (row < 0) {
                result = CIBYL_MKERR(err, CIBYL_EINVAL, "too many rows in fen string");
                goto out;
            }
            sq = row * 8;
        } else {
            /* Any invalid characters return an error. */
            result = CIBYL_MKERR(err, CIBYL_EINVAL, "invalid character in fen body: %c", c);
            goto out;
        }
    }

    /* Throw errors for the write head not being at the end of the board. */
    if (row != 0 && sq != 8) {
        result = CIBYL_MKERR(err, CIBYL_EINVAL, "unexpected end to fen body");
        goto out;
    }

out:
    return result;
}

cibyl_errno_t parse_fen_turn(cibyl_error_t *err, cb_board_t *board, char *fen_turn)
{
    cibyl_errno_t result = CIBYL_EOK;

    /* Error handling. */
    if (fen_turn == NULL) {
        result = CIBYL_MKERR(err, CIBYL_EINVAL, "missing fen turn");
        goto out;
    }

    /* Set the turn in the board. */
    if (*fen_turn == 'w') {
        board->turn = CB_WHITE;
    } else if (*fen_turn == 'b') {
        board->turn = CB_BLACK;
    } else {
        result = CIBYL_MKERR(err, CIBYL_EINVAL, "unexpected character in fen turn");
        goto out;
    }
    
out:
    return result;
}

cibyl_errno_t parse_fen_rights(cibyl_error_t *err, cb_board_t *board, char *fen_rights)
{
    cibyl_errno_t result = CIBYL_EOK;
    int i = 0;
    char c;

    /* Error handling. */
    if (fen_rights == NULL) {
        result = CIBYL_MKERR(err, CIBYL_EINVAL, "missing fen rights");
        goto out;
    }

    /* Set the rights based on the token. */
    while ((c = fen_rights[i++]) != '\0') {
        if (c == 'K') {
            cb_hist_add_ksc(&board->hist.data[board->hist.count - 1].hist, CB_WHITE);
        } else if (c == 'Q') {
            cb_hist_add_qsc(&board->hist.data[board->hist.count - 1].hist, CB_WHITE);
        } else if (c == 'k') {
            cb_hist_add_ksc(&board->hist.data[board->hist.count - 1].hist, CB_BLACK);
        } else if (c == 'q') {
            cb_hist_add_qsc(&board->hist.data[board->hist.count - 1].hist, CB_BLACK);
        } else if (c == '-') {
            result = 0;
            goto out;
        } else {
            result = CIBYL_MKERR(err, CIBYL_EINVAL, "unexpected character in fen rights");
            goto out;
        }
    }

out:
    return result;
}

cibyl_errno_t parse_fen_enp(cibyl_error_t *err, cb_board_t *board, char *fen_enp)
{
    cibyl_errno_t result = CIBYL_EOK;

    if (fen_enp == NULL) {
        result = CIBYL_MKERR(err, CIBYL_EINVAL, "missing fen rights");
        goto out;
    }

    if (strlen(fen_enp) == 1) {
        if (fen_enp[0] != '-') {
            result = CIBYL_MKERR(err, CIBYL_EINVAL, "invalid character in fen enp");
            goto out;
        }
        result = CIBYL_EOK;
        goto out;
    }

    if (strlen(fen_enp) != 2) {
        result = CIBYL_MKERR(err, CIBYL_EINVAL, "invalid fen enpassant length");
        goto out;
    }

    if (fen_enp[0] < 'a' || fen_enp[0] > 'h') {
        result = CIBYL_MKERR(err, CIBYL_EINVAL, "invalid character in fen enp");
        goto out;
    }

    cb_hist_set_enp(&board->hist.data[board->hist.count - 1].hist, fen_enp[0] - 'a');

out:
    return 0;
}

cibyl_errno_t parse_fen_hlfmv(cibyl_error_t *err, cb_board_t *board, char *fen_hlfmv)
{
    cibyl_errno_t result = CIBYL_EOK;
    int hlfmv;
    char *endptr;

    if (fen_hlfmv != NULL) {
        errno = 0;
        hlfmv = strtol(fen_hlfmv, &endptr, 10);
        if (errno || *endptr != '\0') {
            result = CIBYL_MKERR(err, CIBYL_EINVAL, "invalid halfmove number");
            goto out;
        }
        cb_hist_set_halfmove_clk(&board->hist.data[board->hist.count - 1].move, hlfmv);
    }

out:
    return result;
}

cibyl_errno_t cb_board_from_fen(cibyl_error_t *err, cb_board_t *board, char *fen)
{
    cibyl_errno_t result = CIBYL_EOK;
    char *saveptr;

    char *fen_main = strtok_r(fen, " \n", &saveptr);
    char *fen_turn = strtok_r(NULL, " \n", &saveptr);
    char *fen_rights = strtok_r(NULL, " \n", &saveptr);
    char *fen_enp = strtok_r(NULL, " \n", &saveptr);
    char *fen_hlfmv = strtok_r(NULL, " \n", &saveptr);

    /* Reset the board. */
    cb_wipe_board(board);
    if ((result = cb_reserve_for_make(err, board, 1)) != 0) {
        result = CIBYL_MKERR(err, CIBYL_EABORT, "realloc: %s", strerror(errno));
        goto out;
    }
    cb_hist_stack_push(&board->hist, CB_INIT_STATE);

    /* Parse all of the sections of the fen string. */
    if (parse_fen_main(err, board, fen_main) != 0) {
        result = CIBYL_ERR_ADD_CONTEXT(err);
        goto out;
    }
    if (parse_fen_turn(err, board, fen_turn) != 0) {
        result = CIBYL_ERR_ADD_CONTEXT(err);
        goto out;
    }
    if (parse_fen_rights(err, board, fen_rights) != 0) {
        result = CIBYL_ERR_ADD_CONTEXT(err);
        goto out;
    }
    if (parse_fen_enp(err, board, fen_enp) != 0) {
        result = CIBYL_ERR_ADD_CONTEXT(err);
        goto out;
    }
    if (parse_fen_hlfmv(err, board, fen_hlfmv) != 0) {
        result = CIBYL_ERR_ADD_CONTEXT(err);
        goto out;
    }

out:
    return result;
}

cibyl_errno_t cb_board_from_uci(cibyl_error_t *err, cb_board_t *board, char *uci)
{
    cibyl_errno_t result = CIBYL_EOK;
    char *saveptr;
    char *moves;
    char *algbr = NULL;
    cb_move_t mv;

    /* Locate the beginning of the string of moves. */
    if ((moves = strstr(uci, "moves ")) != NULL) {
        *(moves - 1) = '\0';
        moves += 6;
    }

    /* Parse the fen main section. */
    if (cb_board_from_fen(err, board, uci) != CIBYL_EOK) {
        result = CIBYL_ERR_ADD_CONTEXT(err);
        goto out;
    }

    /* If there is no moves string, then move on with your life. */
    if (moves == NULL) {
        result = CIBYL_EOK;
        goto out;
    }

    /* Parse the list of moves. */
    algbr = strtok_r(moves, " \n", &saveptr);
    while (algbr != NULL) {
        if (cb_mv_from_uci_algbr(err, &mv, board, algbr) != 0) {
            result = CIBYL_EOK;
            goto out;
        }
        cb_make(board, mv);
        algbr = strtok_r(NULL, " \n", &saveptr);
    }

out:
    return result;
}

cibyl_errno_t cb_board_from_pgn(cibyl_error_t *err, cb_board_t *board, char *fen)
{
    assert(false && "not yet implemented");
    return 0;
}

cibyl_errno_t cb_board_from_copy(cibyl_error_t *err, cb_board_t *dest, cb_board_t *src)
{
    cibyl_errno_t result = CIBYL_EOK;

    memcpy(dest, src, sizeof(cb_board_t));
    dest->hist.data = (cb_hist_ele_t*)malloc(dest->hist.size * sizeof(cb_hist_ele_t));
    if (dest->hist.data == NULL) {
        result = CIBYL_MKERR(err, CIBYL_ENOMEM, "malloc: %s\n", strerror(errno));
        goto out;
    }
    memcpy(dest->hist.data, src->hist.data, dest->hist.count * sizeof(cb_hist_ele_t));

out:
    return result;
}

