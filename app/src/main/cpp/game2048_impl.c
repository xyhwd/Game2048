#include "game2048.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

// 查表数据
static uint64_t row_left_table[ROW_MAX];
static uint64_t row_right_table[ROW_MAX];
static uint64_t score_table[ROW_MAX];
static int empty_count_table[ROW_MAX];

typedef struct {
    uint64_t key;
    double val;
} trans_entry_t;

// 转置表缓存
static trans_entry_t* trans_table = NULL;

// 初始化游戏表
void init_tables(void) {
    int i, j;
    uint16_t row, rev_row, res_row, res_rev_row;
    uint16_t score;

    // 分配转置表内存
    if (trans_table == NULL) {
        trans_table = (trans_entry_t*)calloc(TRANSTABLE_SIZE, sizeof(trans_entry_t));
    }

    // 初始化所有表格
    for (row = 0; row < ROW_MAX; row++) {
        // 左移表
        int line[4] = {
            (row >> 0) & 0xf,
            (row >> 4) & 0xf,
            (row >> 8) & 0xf,
            (row >> 12) & 0xf
        };
        score = 0;
        for (i = 0; i < 3; i++) {
            j = i + 1;
            while (j < 4) {
                if (line[j] == 0) {
                    j++;
                    continue;
                }
                if (line[i] == 0) {
                    line[i] = line[j];
                    line[j] = 0;
                    i--;
                    break;
                } else if (line[i] == line[j]) {
                    line[i]++;
                    score += (1 << line[i]);
                    line[j] = 0;
                }
                break;
            }
        }

        res_row = (line[0] << 0) | (line[1] << 4) | (line[2] << 8) | (line[3] << 12);
        row_left_table[row] = res_row;
        score_table[row] = score;

        // 计算空格数
        empty_count_table[row] = 0;
        for (i = 0; i < 4; i++) {
            if (((row >> (i * 4)) & 0xf) == 0) {
                empty_count_table[row]++;
            }
        }

        // 右移表 (反转行)
        rev_row = ((row >> 0) & 0xf) << 12 | 
                 ((row >> 4) & 0xf) << 8 | 
                 ((row >> 8) & 0xf) << 4 | 
                 ((row >> 12) & 0xf);
        res_rev_row = row_left_table[rev_row];
        res_row = ((res_rev_row >> 0) & 0xf) << 12 | 
                 ((res_rev_row >> 4) & 0xf) << 8 | 
                 ((res_rev_row >> 8) & 0xf) << 4 | 
                 ((res_rev_row >> 12) & 0xf);
        row_right_table[row] = res_row;
    }
}

// 执行一步移动
uint64_t execute_move(int move, uint64_t board) {
    uint64_t ret = 0;
    uint16_t row;
    int i;

    switch (move) {
        case UP:
            board = transpose(board);
            for (i = 0; i < BOARD_SIZE; i++) {
                row = (board >> (i * 16)) & ROW_MASK;
                ret |= ((uint64_t)row_left_table[row]) << (i * 16);
            }
            return transpose(ret);
        case DOWN:
            board = transpose(board);
            for (i = 0; i < BOARD_SIZE; i++) {
                row = (board >> (i * 16)) & ROW_MASK;
                ret |= ((uint64_t)row_right_table[row]) << (i * 16);
            }
            return transpose(ret);
        case LEFT:
            for (i = 0; i < BOARD_SIZE; i++) {
                row = (board >> (i * 16)) & ROW_MASK;
                ret |= ((uint64_t)row_left_table[row]) << (i * 16);
            }
            return ret;
        case RIGHT:
            for (i = 0; i < BOARD_SIZE; i++) {
                row = (board >> (i * 16)) & ROW_MASK;
                ret |= ((uint64_t)row_right_table[row]) << (i * 16);
            }
            return ret;
        default:
            return board;
    }
}

// 计算空格数
int count_empty(uint64_t board) {
    int empty = 0;
    int i;
    for (i = 0; i < BOARD_SIZE; i++) {
        empty += empty_count_table[(board >> (i * 16)) & ROW_MASK];
    }
    return empty;
}

// 转置棋盘
uint64_t transpose(uint64_t board) {
    uint64_t a1 = board & 0xF0F00F0FF0F00F0FULL;
    uint64_t a2 = board & 0x0000F0F00000F0F0ULL;
    uint64_t a3 = board & 0x0F0F00000F0F0000ULL;
    uint64_t a = a1 | (a2 << 12) | (a3 >> 12);
    uint64_t b1 = a & 0xFF00FF0000FF00FFULL;
    uint64_t b2 = a & 0x00FF00FF00000000ULL;
    uint64_t b3 = a & 0x00000000FF00FF00ULL;
    return b1 | (b2 >> 24) | (b3 << 24);
}

// 获取最大砖块等级
int get_max_rank(uint64_t board) {
    int maxrank = 0;
    uint64_t t = board;
    while (t) {
        maxrank = maxrank > (t & 0xf) ? maxrank : (t & 0xf);
        t >>= 4;
    }
    return maxrank;
}

// 添加随机砖块
uint64_t add_random_tile(uint64_t board) {
    int empty = count_empty(board);
    if (empty == 0) return board;
    
    int position = rand() % empty;
    uint64_t tmp = board;
    int tile_value;
    int max_rank = get_max_rank(board);
    
    // 根据当前最大砖块值调整新砖块生成概率
    double r = (double)rand() / RAND_MAX;
    
    if (max_rank < 9) {  // 512以下
        if (r < 0.6) {
            tile_value = 1;  // 2^1 = 2
        } else if (r < 0.9) {
            tile_value = 2;  // 2^2 = 4
        } else {
            tile_value = 3;  // 2^3 = 8
        }
    } else if (max_rank < 10) {  // 512-1024
        if (r < 0.57) {
            tile_value = 1;  // 2
        } else if (r < 0.87) {
            tile_value = 2;  // 4
        } else if (r < 0.97) {
            tile_value = 3;  // 8
        } else {
            tile_value = 4;  // 16
        }
    } else {  // 1024及以上
        if (r < 0.54) {
            tile_value = 1;  // 2
        } else if (r < 0.84) {
            tile_value = 2;  // 4
        } else if (r < 0.94) {
            tile_value = 3;  // 8
        } else if (r < 0.97) {
            tile_value = 4;  // 16
        } else {
            tile_value = 5;  // 32
        }
    }
    
    int i, j;
    for (i = 0; i < 16; i++) {
        int shift = i * 4;
        if (((board >> shift) & 0xF) == 0) {
            if (position == 0) {
                return board | ((uint64_t)tile_value << shift);
            }
            position--;
        }
    }
    
    return board;
}

// 棋盘转换为网格
void board_to_grid(uint64_t board, int grid[BOARD_SIZE][BOARD_SIZE]) {
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            int shift = (i * BOARD_SIZE + j) * 4;
            int val = (board >> shift) & 0xF;
            grid[i][j] = val == 0 ? 0 : (1 << val);
        }
    }
}

// 网格转换为棋盘
uint64_t grid_to_board(int grid[BOARD_SIZE][BOARD_SIZE]) {
    uint64_t board = 0;
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            int val = grid[i][j];
            int rank = 0;
            while (val > 1) {
                val >>= 1;
                rank++;
            }
            int shift = (i * BOARD_SIZE + j) * 4;
            board |= ((uint64_t)(val == 0 ? 0 : rank) << shift);
        }
    }
    return board;
}

// 游戏状态函数
void init_game(GameState* state) {
    state->board = 0;
    state->score = 0;
    state->game_over = false;
    
    // 初始添加两个砖块
    state->board = add_random_tile(state->board);
    state->board = add_random_tile(state->board);
}

// 执行上移操作
bool move_up(GameState* state) {
    uint64_t new_board = execute_move(UP, state->board);
    if (new_board == state->board) {
        return false;
    }
    
    // 计算得分
    int i;
    for (i = 0; i < BOARD_SIZE; i++) {
        uint16_t row = (state->board >> (i * 16)) & ROW_MASK;
        uint16_t new_row = (new_board >> (i * 16)) & ROW_MASK;
        state->score += score_table[row];
    }
    
    // 更新最高分
    if (state->score > state->best_score) {
        state->best_score = state->score;
    }
    
    // 更新棋盘并添加新砖块
    state->board = add_random_tile(new_board);
    
    // 检查游戏结束
    state->game_over = is_game_over(state);
    
    return true;
}

// 执行下移操作
bool move_down(GameState* state) {
    uint64_t new_board = execute_move(DOWN, state->board);
    if (new_board == state->board) {
        return false;
    }
    
    // 计算得分
    int i;
    for (i = 0; i < BOARD_SIZE; i++) {
        uint16_t row = (state->board >> (i * 16)) & ROW_MASK;
        uint16_t new_row = (new_board >> (i * 16)) & ROW_MASK;
        state->score += score_table[row];
    }
    
    // 更新最高分
    if (state->score > state->best_score) {
        state->best_score = state->score;
    }
    
    // 更新棋盘并添加新砖块
    state->board = add_random_tile(new_board);
    
    // 检查游戏结束
    state->game_over = is_game_over(state);
    
    return true;
}

// 执行左移操作
bool move_left(GameState* state) {
    uint64_t new_board = execute_move(LEFT, state->board);
    if (new_board == state->board) {
        return false;
    }
    
    // 计算得分
    int i;
    for (i = 0; i < BOARD_SIZE; i++) {
        uint16_t row = (state->board >> (i * 16)) & ROW_MASK;
        uint16_t new_row = (new_board >> (i * 16)) & ROW_MASK;
        state->score += score_table[row];
    }
    
    // 更新最高分
    if (state->score > state->best_score) {
        state->best_score = state->score;
    }
    
    // 更新棋盘并添加新砖块
    state->board = add_random_tile(new_board);
    
    // 检查游戏结束
    state->game_over = is_game_over(state);
    
    return true;
}

// 执行右移操作
bool move_right(GameState* state) {
    uint64_t new_board = execute_move(RIGHT, state->board);
    if (new_board == state->board) {
        return false;
    }
    
    // 计算得分
    int i;
    for (i = 0; i < BOARD_SIZE; i++) {
        uint16_t row = (state->board >> (i * 16)) & ROW_MASK;
        uint16_t new_row = (new_board >> (i * 16)) & ROW_MASK;
        state->score += score_table[row];
    }
    
    // 更新最高分
    if (state->score > state->best_score) {
        state->best_score = state->score;
    }
    
    // 更新棋盘并添加新砖块
    state->board = add_random_tile(new_board);
    
    // 检查游戏结束
    state->game_over = is_game_over(state);
    
    return true;
}

// 检查游戏是否结束
bool is_game_over(GameState* state) {
    if (count_empty(state->board) > 0) {
        return false;
    }
    
    // 检查是否有可以合并的砖块
    uint64_t board = state->board;
    uint64_t new_board;
    
    new_board = execute_move(UP, board);
    if (new_board != board) return false;
    
    new_board = execute_move(DOWN, board);
    if (new_board != board) return false;
    
    new_board = execute_move(LEFT, board);
    if (new_board != board) return false;
    
    new_board = execute_move(RIGHT, board);
    if (new_board != board) return false;
    
    return true;
}

// AI评估
double score_heur_board(uint64_t board) {
    int empty = count_empty(board);
    double score = SCORE_EMPTY_WEIGHT * empty;
    
    // TODO: 实现更复杂的评估函数
    
    return score;
}

// 获取最佳移动方向
int find_best_move(GameState* state, int depth_limit) {
    int best_move = -1;
    double best_score = -INFINITY;
    double scores[4];
    
    // 简单版本，尝试每个方向，选择得分最高的
    for (int move = 0; move < 4; move++) {
        uint64_t new_board = execute_move(move, state->board);
        if (new_board != state->board) {
            double score = score_heur_board(new_board);
            if (score > best_score) {
                best_score = score;
                best_move = move;
            }
        }
    }
    
    return best_move;
} 