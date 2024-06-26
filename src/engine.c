#include <stdbool.h>
#include <stdio.h>

#include "consts.h"
#include "engine.h"

typedef enum {
    NORTH = 8,
    EAST = 1,
    SOUTH = -8,
    WEST = -1,
    NORTH_WEST = 7,
    NORTH_EAST = 9,
    SOUTH_WEST = -9,
    SOUTH_EAST = -7
} Direction;

static const Direction directions[4] = {
    NORTH,
    EAST,
    SOUTH,
    WEST,
};

static const Direction diagonals[4] = {
    NORTH_WEST,
    NORTH_EAST,
    SOUTH_WEST,
    SOUTH_EAST,
};

typedef struct {
    Bitboard lower;
    Bitboard upper;
    Bitboard line_ex; // lower | upper
} SMasks;             // 6 (4) KByte, needs initialization

static SMasks masks[BOARD_SIZE][4];

#define FILE_OF(S) ((S) & 7)
#define GET_LOWER(S) ((1ULL << S) - 1)
#define GET_UPPER(S) (0xFFFFFFFFFFFFFFFFULL << (S))

inline void bit_clear(Bitboard *number, Bitboard n) {
    *number &= ~(1ULL << n);
}

inline void bit_set(Bitboard *number, Bitboard n) {
    *number |= (1ULL << n);
}

// Print the board represented by the given bitboard
void print_board(Bitboard board) {
    printf("\n");
    for (size_t sq = A1; sq <= H8; sq++) {
        if (board & (1ULL << sq)) {
            printf("1 ");
        } else {
            printf("0 ");
        }

        if (FILE_OF(sq) == 7) {
            printf("\n");
        }
    }
}

const Bitboard not_a_file = 0xfefefefefefefefeULL;
const Bitboard not_h_file = 0x7f7f7f7f7f7f7f7fULL;
const Bitboard not_ab_file = 0xfcfcfcfcfcfcfcfcULL;
const Bitboard not_gh_file = 0x3f3f3f3f3f3f3f3fULL;

inline Bitboard sout_one(Bitboard b) {
    return b >> 8;
}

inline Bitboard nort_one(Bitboard b) {
    return b << 8;
}

inline Bitboard east_one(Bitboard b) {
    return (b << 1) & not_a_file;
}

inline Bitboard no_ea_one(Bitboard b) {
    return (b << 9) & not_a_file;
}

inline Bitboard so_ea_one(Bitboard b) {
    return (b >> 7) & not_a_file;
}

inline Bitboard west_one(Bitboard b) {
    return (b >> 1) & not_h_file;
}

inline Bitboard so_we_one(Bitboard b) {
    return (b >> 9) & not_h_file;
}

inline Bitboard no_we_one(Bitboard b) {
    return (b << 7) & not_h_file;
}

inline Bitboard rotate_left(Bitboard x, int s) {
    return (x << s) | (x >> (BOARD_SIZE - s));
}

inline Bitboard rotate_right(Bitboard x, int s) {
    return (x >> s) | (x << (BOARD_SIZE - s));
}

inline Bitboard w_single_push_targets(Bitboard wpawns, Bitboard occ) {
    return nort_one(wpawns) & ~occ;
}

Bitboard w_dbl_push_targets(Bitboard wpawns, Bitboard occ) {
    Bitboard single_pushs = w_single_push_targets(wpawns, occ);
    return nort_one(single_pushs) & ~occ & RANK_4;
}

inline Bitboard b_single_push_targets(Bitboard bpawns, Bitboard occ) {
    return sout_one(bpawns) & ~occ;
}

Bitboard b_dbl_push_targets(Bitboard bpawns, Bitboard occ) {
    Bitboard single_pushs = b_single_push_targets(bpawns, occ);
    return sout_one(single_pushs) & ~occ & RANK_5;
}

void generate_pawn_moves(State *game, size_t *num_moves) {
    Bitboard white_pawns = game->bit_boards[0];
    Bitboard black_pawns = game->bit_boards[6];
    Bitboard all_pieces = game->all_pieces;

    Bitboard white_single_pushes =
        w_single_push_targets(white_pawns, all_pieces);
    Bitboard black_single_pushes =
        b_single_push_targets(black_pawns, all_pieces);

    Bitboard white_dbl_pushes = w_dbl_push_targets(white_pawns, all_pieces);
    Bitboard black_dbl_pushes = b_dbl_push_targets(black_pawns, all_pieces);

    for (Square sq = A1; sq <= H8; sq++) {
        if (white_single_pushes & (1ULL << sq)) {
            game->moves[(*num_moves)++] = (Move){sq - 8, sq, QUIET, PAWN};
        } else if (black_single_pushes & (1ULL << sq)) {
            game->moves[(*num_moves)++] = (Move){sq + 8, sq, QUIET, PAWN};
        } else if (white_dbl_pushes & (1ULL << sq)) {
            game->moves[(*num_moves)++] =
                (Move){sq - 16, sq, DOUBLE_PAWN_PUSH, PAWN};
        } else if (black_dbl_pushes & (1ULL << sq)) {
            game->moves[(*num_moves)++] =
                (Move){sq + 16, sq, DOUBLE_PAWN_PUSH, PAWN};
        }
    }
}

Bitboard knight_attacks(Bitboard knights) {
    Bitboard l1 = (knights >> 1) & not_h_file;
    Bitboard l2 = (knights >> 2) & not_gh_file;
    Bitboard r1 = (knights << 1) & not_a_file;
    Bitboard r2 = (knights << 2) & not_ab_file;
    Bitboard h1 = l1 | r1;
    Bitboard h2 = l2 | r2;
    return (h1 << 16) | (h1 >> 16) | (h2 << 8) | (h2 >> 8);
}

Bitboard king_attacks(Bitboard kingSet) {
    Bitboard attacks = east_one(kingSet) | west_one(kingSet);
    kingSet |= attacks;
    attacks |= nort_one(kingSet) | sout_one(kingSet);
    return attacks;
}

void generate_king_moves(State *game, size_t *num_moves) {
    Bitboard kings = game->bit_boards[5] | game->bit_boards[11];

    for (Square sq = A1; sq <= H8; sq++) {
        if (kings & (1ULL << sq)) {
            Bitboard potential_moves = king_attacks(kings);
            Bitboard occupied_squares = potential_moves & game->all_pieces;
            Bitboard king_moves = potential_moves & ~occupied_squares;
            for (int to_sq = 0; to_sq < BOARD_SIZE; to_sq++) {
                if (king_moves & (1ULL << to_sq)) {
                    game->moves[(*num_moves)++] =
                        (Move){sq, to_sq, QUIET, KING};
                }
            }
        }
    }
}

void generate_knight_moves(State *game, size_t *num_moves) {
    Bitboard knights = game->bit_boards[1] | game->bit_boards[7];

    for (Square sq = A1; sq <= H8; sq++) {
        if (knights & (1ULL << sq)) {
            Bitboard potential_moves = knight_attacks((1ULL << sq));
            Bitboard occupied_squares = potential_moves & game->all_pieces;
            Bitboard knight_moves = potential_moves & ~occupied_squares;
            for (int to_sq = 0; to_sq < BOARD_SIZE; to_sq++) {
                if (knight_moves & (1ULL << to_sq)) {
                    game->moves[(*num_moves)++] =
                        (Move){sq, to_sq, QUIET, KNIGHT};
                }
            }
        }
    }
}

inline Bitboard init_rank(int s) {
    return 0xFFULL << (s & 56);
}

inline Bitboard init_file(int s) {
    return A_FILE << FILE_OF(s);
}

Bitboard init_diag(int sq) {
    int d = 8 * FILE_OF(sq) - (sq & 56);
    int n = -d & (d >> 31);
    int s = d & (-d >> 31);
    return (DIAG_MASK >> s) << n;
}

Bitboard init_anti(int sq) {
    int d = 56 - 8 * FILE_OF(sq) - (sq & 56);
    int n = -d & (d >> 31);
    int s = d & (-d >> 31);
    return (ANTI_DIAG_MASK >> s) << n;
}

inline Bitboard init_low(int sq, uint64_t line) {
    return GET_LOWER(sq) & (line ^ (1ULL << sq));
}

inline Bitboard init_up(int sq, uint64_t line) {
    return GET_UPPER(sq) & (line ^ (1ULL << sq));
}

SMasks new_mask(int sq, Bitboard line) {
    SMasks masks;
    masks.lower = init_low(sq, line);
    masks.upper = init_up(sq, line);
    masks.line_ex = init_low(sq, line) | init_up(sq, line);
    return masks;
}

void init_masks() {
    for (int sq = 0; sq < BOARD_SIZE; sq++) {
        masks[sq][0] = new_mask(sq, init_rank(sq));
        masks[sq][1] = new_mask(sq, init_file(sq));
        masks[sq][2] = new_mask(sq, init_diag(sq));
        masks[sq][3] = new_mask(sq, init_anti(sq));
    }
}

/*
 * @author Michael Hoffmann, Daniel Infuehr
 * @param  Bitboard occ occupancy of the board
 * @param  SMasks* a mask structure by square and line
 * @return line attacks from that square
 */
Bitboard line_attacks(Bitboard lower, Bitboard upper, Bitboard mask) {
    Bitboard ms1b = 0x8000000000000000ULL >> __builtin_clzl(lower | 1);
    return mask & (upper - ms1b);
}

void generate_attacks_by_ray(int square, Direction direction, Bitboard occ,
                             Bitboard *lines) {
    int current_square = square;
    bool found_obstruction = false;

    for (int i = 0; i < 8; i++) {
        current_square += direction;

        if (current_square < 0 || current_square >= BOARD_SIZE) {
            break;
        }

        // could maybe combine these two checks
        if (occ & (1ULL << current_square)) {
            found_obstruction = true;
        }

        if (found_obstruction) {
            bit_clear(lines, current_square);
        }
    }
}

Bitboard sliding_attacks(Bitboard pieces, Bitboard occ, int sq, Piece piece) {
    SMasks *r = masks[sq];

    Bitboard lines = EMPTY_BOARD;

    if (piece == ROOK) {
        lines = line_attacks(r[0].lower & pieces, r[0].upper & pieces,
                             r[0].line_ex) |
                line_attacks(r[1].lower & pieces, r[1].upper & pieces,
                             r[1].line_ex);
    } else if (piece == BISHOP) {
        lines = line_attacks(r[2].lower & pieces, r[2].upper & pieces,
                             r[2].line_ex) |
                line_attacks(r[3].lower & pieces, r[3].upper & pieces,
                             r[3].line_ex);
    }

    if (lines == EMPTY_BOARD) {
        return lines;
    }

    for (int i = 0; i < 4; i++) {
        if (piece == ROOK) {
            generate_attacks_by_ray(sq, directions[i], occ, &lines);
        } else {
            generate_attacks_by_ray(sq, diagonals[i], occ, &lines);
        }
    }

    return lines;
}

Bitboard queen_attacks(Bitboard pieces, Bitboard occ, int sq) {
    return sliding_attacks(pieces, occ, sq, ROOK) |
           sliding_attacks(pieces, occ, sq, BISHOP);
}

void generate_sliding_moves(State *game, size_t *num_moves, Piece piece) {
    Bitboard pieces = EMPTY_BOARD;
    switch (piece) {
    case ROOK:
        pieces = game->bit_boards[3] | game->bit_boards[9];
        break;
    case BISHOP:
        pieces = game->bit_boards[2] | game->bit_boards[8];
        break;
    case QUEEN:
        pieces = game->bit_boards[4] | game->bit_boards[10];
        break;
    default:
        break;
    }

    for (Square sq = A1; sq <= H8; sq++) {
        if (pieces & (1ULL << sq)) {
            Bitboard piece_moves = 0;
            if (piece == ROOK || piece == BISHOP) {
                piece_moves =
                    sliding_attacks(pieces, game->all_pieces, sq, piece);
            } else if (piece == QUEEN) {
                piece_moves = queen_attacks(pieces, game->all_pieces, sq);
            }

            for (int to_sq = 0; to_sq < BOARD_SIZE; to_sq++) {
                if (piece_moves & (1ULL << to_sq)) {
                    game->moves[(*num_moves)++] =
                        (Move){sq, to_sq, QUIET, piece};
                }
            }
        }
    }
}

inline Move empty_move() {
    return (Move){EMPTY, EMPTY, QUIET, NONE};
}

void generate_moves(State *game) {
    for (size_t i = 0; i < MAX_MOVES; i++) {
        game->moves[i] = empty_move();
    }

    size_t num_moves = 0;

    generate_pawn_moves(game, &num_moves);
    generate_knight_moves(game, &num_moves);
    generate_king_moves(game, &num_moves);
    generate_sliding_moves(game, &num_moves, ROOK);
    generate_sliding_moves(game, &num_moves, BISHOP);
    generate_sliding_moves(game, &num_moves, QUEEN);

    printf("Number of moves: %lu\n", num_moves);
    // for (size_t i = 0; i < num_moves; i++) {
    //     printf("From: %d, To: %d\n", game->moves[i].from, game->moves[i].to);
    // }
}

void move_piece(State *game, Move *move) {
    Bitboard from = 1ULL << move->from;
    Bitboard to = 1ULL << move->to;

    for (size_t i = 0; i < PIECE_TYPE_COUNT; i++) {
        if (game->bit_boards[i] & from) {
            game->bit_boards[i] &= ~from;
            game->bit_boards[i] |= to;

            print_board(game->bit_boards[i]);
        }
    }

    game->all_pieces &= ~from;
    game->all_pieces |= to;

    print_board(game->all_pieces);
}

static const Bitboard pieces[] = {
    WHITE_PAWN, WHITE_KNIGHT, WHITE_BISHOP, WHITE_ROOK, WHITE_KING, WHITE_QUEEN,
    BLACK_PAWN, BLACK_KNIGHT, BLACK_BISHOP, BLACK_ROOK, BLACK_KING, BLACK_QUEEN,
};

State new_state() {
    State game = {0};

    game.all_pieces = EMPTY_BOARD;
    for (size_t i = 0; i < PIECE_TYPE_COUNT; i++) {
        game.bit_boards[i] = EMPTY_BOARD | pieces[i];
        game.all_pieces |= game.bit_boards[i];
    }

    init_masks();

    return game;
}
