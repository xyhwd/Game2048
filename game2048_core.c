// game2048_core.c - 核心游戏逻辑实现
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdbool.h>
#include "game2048.h"

// 添加max宏定义
#define max(a,b) ((a) > (b) ? (a) : (b))
#define min(a,b) ((a) < (b) ? (a) : (b))

// 函数前向声明
double score_tilechoose_node(EvalState *state, uint64_t board, double cprob);
double score_move_node(EvalState *state, uint64_t board, double cprob);
double score_heur_board(uint64_t board);

// 移动表和得分表
static uint64_t row_left_table[ROW_MAX];
static uint64_t row_right_table[ROW_MAX];
static uint64_t col_up_table[ROW_MAX];
static uint64_t col_down_table[ROW_MAX];
static double heur_score_table[ROW_MAX];
static double score_table[ROW_MAX];

// 哈希表实现 (简化版)
typedef struct TransEntry {
    uint64_t key;
    int depth;
    double score;
    struct TransEntry* next;
} TransEntry;

typedef struct {
    TransEntry** entries;
    size_t size;
    size_t count;
} TransTable;

// 创建转置表
TransTable* create_trans_table(size_t size) {
    TransTable* table = (TransTable*)malloc(sizeof(TransTable));
    if (!table) return NULL;
    
    table->size = size;
    table->count = 0;
    table->entries = (TransEntry**)calloc(size, sizeof(TransEntry*));
    if (!table->entries) {
        free(table);
        return NULL;
    }
    
    return table;
}

// 简易哈希函数
size_t hash_function(uint64_t key, size_t size) {
    // 简单的哈希函数：取模
    return key % size;
}

// 在转置表中查找
TransEntry* find_in_table(TransTable* table, uint64_t key) {
    size_t index = hash_function(key, table->size);
    TransEntry* entry = table->entries[index];
    
    while (entry != NULL) {
        if (entry->key == key) {
            return entry;
        }
        entry = entry->next;
    }
    
    return NULL;
}

// 向转置表中插入
void insert_to_table(TransTable* table, uint64_t key, int depth, double score) {
    size_t index = hash_function(key, table->size);
    
    // 检查是否已存在
    TransEntry* existing = find_in_table(table, key);
    if (existing) {
        // 更新现有条目
        existing->depth = depth;
        existing->score = score;
        return;
    }
    
    // 创建新条目
    TransEntry* entry = (TransEntry*)malloc(sizeof(TransEntry));
    if (!entry) return;
    
    entry->key = key;
    entry->depth = depth;
    entry->score = score;
    entry->next = table->entries[index];
    table->entries[index] = entry;
    table->count++;
}

// 释放转置表
void free_trans_table(TransTable* table) {
    if (!table) return;
    
    for (size_t i = 0; i < table->size; i++) {
        TransEntry* entry = table->entries[i];
        while (entry) {
            TransEntry* next = entry->next;
            free(entry);
            entry = next;
        }
    }
    
    free(table->entries);
    free(table);
}

// 位操作辅助函数
static uint64_t reverse_row(uint64_t row) {
    return ((row >> 12) & 0xF) | ((row >> 4) & 0xF0) | ((row << 4) & 0xF00) | ((row << 12) & 0xF000);
}

static uint64_t unpack_col(uint64_t row) {
    return ((row & 0xF) | ((row & 0xF0) << 12) |
            ((row & 0xF00) << 24) | ((row & 0xF000) << 36));
}

int get_max_rank(uint64_t board) {
    int maxrank = 0;
    while (board) {
        int current = board & 0xf;
        maxrank = current > maxrank ? current : maxrank;
        board >>= 4;
    }
    return maxrank;
}

// 初始化表格
void init_tables(void) {
    for (unsigned row = 0; row < ROW_MAX; row++) {
        unsigned line[4] = {
            (row >> 0) & 0xf,
            (row >> 4) & 0xf,
            (row >> 8) & 0xf,
            (row >> 12) & 0xf
        };

        // 分数表计算
        double score = 0.0;
        for (int i = 0; i < 4; i++) {
            unsigned rank = line[i];
            if (rank >= 2) {
                score += (rank - 1) * (1 << rank);
            }
        }
        score_table[row] = score;

        // 启发式分数计算
        double sum = 0;
        int empty = 0;
        int merges = 0;
        int prev = 0;
        int counter = 0;

        for (int i = 0; i < 4; i++) {
            unsigned rank = line[i];
            sum += pow(rank, SCORE_SUM_POWER);
            if (rank == 0) {
                empty++;
            } else {
                if (prev == rank) {
                    counter++;
                } else if (counter > 0) {
                    merges += 1 + counter;
                    counter = 0;
                }
                prev = rank;
            }
        }
        if (counter > 0) merges += 1 + counter;

        double monotonicity_left = 0, monotonicity_right = 0;
        for (int i = 1; i < 4; i++) {
            if (line[i-1] > line[i]) {
                monotonicity_left += pow(line[i-1], SCORE_MONOTONICITY_POWER) - pow(line[i], SCORE_MONOTONICITY_POWER);
            } else {
                monotonicity_right += pow(line[i], SCORE_MONOTONICITY_POWER) - pow(line[i-1], SCORE_MONOTONICITY_POWER);
            }
        }

        heur_score_table[row] = SCORE_LOST_PENALTY +
                              SCORE_EMPTY_WEIGHT * empty +
                              SCORE_MERGES_WEIGHT * merges -
                              SCORE_MONOTONICITY_WEIGHT * fmin(monotonicity_left, monotonicity_right) -
                              SCORE_SUM_WEIGHT * sum;

        // 关键修改：合并逻辑限制到MAX_RANK (2048)
        unsigned result_line[4] = {line[0], line[1], line[2], line[3]};
        int i = 0;
        while (i < 3) {
            int j = i + 1;
            while (j < 4 && result_line[j] == 0) j++;  // 跳过右侧空位
            if (j == 4) break;  // 没有更多砖块

            if (result_line[i] == 0) {  // 移动空位到左侧
                result_line[i] = result_line[j];
                result_line[j] = 0;
                i--;  // 重新检查当前位置
            } else if (result_line[i] == result_line[j] && result_line[i] < MAX_RANK) {  // 合并条件：相同且小于MAX_RANK
                result_line[i]++;  // 合并后的等级+1（比如10->11）
                result_line[j] = 0;  // 清除右侧砖块
            }
            i++;
        }

        // 生成移动表
        uint64_t result = (result_line[0] << 0) | (result_line[1] << 4) | 
                         (result_line[2] << 8) | (result_line[3] << 12);
        uint64_t rev_result = reverse_row(result);
        uint64_t rev_row = reverse_row(row);

        row_left_table[row] = row ^ result;
        row_right_table[rev_row] = rev_row ^ rev_result;
        col_up_table[row] = unpack_col(row) ^ unpack_col(result);
        col_down_table[rev_row] = unpack_col(rev_row) ^ unpack_col(rev_result);
    }
}

// 移动执行函数
uint64_t execute_move_0(uint64_t board) {
    uint64_t ret = board;
    uint64_t t = transpose(board);
    ret ^= (col_up_table[(t >> 0) & ROW_MASK] << 0);
    ret ^= (col_up_table[(t >> 16) & ROW_MASK] << 4);
    ret ^= (col_up_table[(t >> 32) & ROW_MASK] << 8);
    ret ^= (col_up_table[(t >> 48) & ROW_MASK] << 12);
    return ret;
}

uint64_t execute_move_1(uint64_t board) {
    uint64_t ret = board;
    uint64_t t = transpose(board);
    ret ^= (col_down_table[(t >> 0) & ROW_MASK] << 0);
    ret ^= (col_down_table[(t >> 16) & ROW_MASK] << 4);
    ret ^= (col_down_table[(t >> 32) & ROW_MASK] << 8);
    ret ^= (col_down_table[(t >> 48) & ROW_MASK] << 12);
    return ret;
}

uint64_t execute_move_2(uint64_t board) {
    uint64_t ret = board;
    ret ^= (row_left_table[(board >> 0) & ROW_MASK] << 0);
    ret ^= (row_left_table[(board >> 16) & ROW_MASK] << 16);
    ret ^= (row_left_table[(board >> 32) & ROW_MASK] << 32);
    ret ^= (row_left_table[(board >> 48) & ROW_MASK] << 48);
    return ret;
}

uint64_t execute_move_3(uint64_t board) {
    uint64_t ret = board;
    ret ^= (row_right_table[(board >> 0) & ROW_MASK] << 0);
    ret ^= (row_right_table[(board >> 16) & ROW_MASK] << 16);
    ret ^= (row_right_table[(board >> 32) & ROW_MASK] << 32);
    ret ^= (row_right_table[(board >> 48) & ROW_MASK] << 48);
    return ret;
}

uint64_t execute_move(int move, uint64_t board) {
    switch(move) {
        case UP:    return execute_move_0(board);
        case DOWN:  return execute_move_1(board);
        case LEFT:  return execute_move_2(board);
        case RIGHT: return execute_move_3(board);
    }
    return board; // 无效移动
}

double score_helper(uint64_t board, double *table) {
    return table[(board >>  0) & ROW_MASK] +
           table[(board >> 16) & ROW_MASK] +
           table[(board >> 32) & ROW_MASK] +
           table[(board >> 48) & ROW_MASK];
}

double score_heur_board(uint64_t board) {
    // 评估原始棋盘和转置棋盘的启发式得分
    return score_helper(board, heur_score_table) + score_helper(transpose(board), heur_score_table);
}

double score_board(uint64_t board) {
    return score_helper(board, score_table);
}

uint64_t transpose(uint64_t x) {
    uint64_t a1 = x & 0xF0F00F0FF0F00F0FULL;
    uint64_t a2 = x & 0x0000F0F00000F0F0ULL;
    uint64_t a3 = x & 0x0F0F00000F0F0000ULL;
    uint64_t a = a1 | (a2 << 12) | (a3 >> 12);
    uint64_t b1 = a & 0xFF00FF0000FF00FFULL;
    uint64_t b2 = a & 0x00FF00FF00000000ULL;
    uint64_t b3 = a & 0x00000000FF00FF00ULL;
    return b1 | (b2 >> 24) | (b3 << 24);
}

int count_empty(uint64_t x) {
    x |= (x >> 2) & 0x3333333333333333ULL;
    x |= (x >> 1);
    x = ~x & 0x1111111111111111ULL;
    x += x >> 32;
    x += x >> 16;
    x += x >> 8;
    x += x >> 4;
    return x & 0xf;
}

double score_tilechoose_node(EvalState *state, uint64_t board, double cprob) {
    // 深度限制和概率剪枝
    if (cprob < CPROB_THRESH_BASE || state->curdepth >= state->depth_limit) {
        state->maxdepth = max(state->maxdepth, state->curdepth);
        return score_heur_board(board);
    }

    // 使用转置表缓存结果
    if (state->curdepth < CACHE_DEPTH_LIMIT) {
        TransEntry *entry = find_in_table(state->trans_table, board);
        if (entry != NULL && entry->depth <= state->curdepth) {
            state->cachehits++;
            return entry->score;
        }
    }

    // 获取空位数量
    int num_empty = count_empty(board);
    if (num_empty == 0) {
        return SCORE_LOST_PENALTY;  // 如果没有空位，游戏结束
    }

    // 调整概率
    cprob /= num_empty;
    
    double res = 0.0;
    
    // 获取当前棋盘最大砖块的幂
    int maxrank = get_max_rank(board);
    
    // 根据不同的棋盘状态调整砖块出现概率和考虑的砖块种类
    double prob_2 = 0.9;
    double prob_4 = 0.1;
    
    // 砖块种类的考虑根据最大砖块值调整
    if (maxrank >= 10) { // >= 1024 (2^10)
        // 棋盘上有大砖块时，考虑更多种类的新砖块
        prob_2 = 0.54;  // 与Python版本一致
        prob_4 = 0.3;
    } else if (maxrank >= 9) { // >= 512 (2^9)
        prob_2 = 0.57;
        prob_4 = 0.3;
    } else {
        prob_2 = 0.6;
        prob_4 = 0.3;
    }
    
    // 采样密度策略：更智能地采样
    int max_samples;
    if (num_empty <= 6) {
        // 当空位较少时，考虑全部空位
        max_samples = num_empty;
    } else {
        // 当空位较多时，使用采样
        max_samples = 6 + (num_empty > 10 ? 2 : 1);
        if (max_samples > 10) max_samples = 10; // 平衡性能和精度
    }

    // 扫描所有可能的位置
    int sample_count = 0;
    
    for (int pos = 0; pos < 16; pos++) {
        // 检查位置是否为空
        if (((board >> (pos * 4)) & 0xF) == 0) {
            // 计算是否需要采样这个位置
            if (num_empty <= 6 || (sample_count * max_samples) / num_empty != ((sample_count + 1) * max_samples) / num_empty) {
                // 考虑2砖块 (2^1)
                double score2 = score_move_node(state, board | (1ULL << (pos * 4)), cprob * prob_2);
                
                // 考虑4砖块 (2^2)
                double score4 = score_move_node(state, board | (2ULL << (pos * 4)), cprob * prob_4);
                
                // 如果是深度较大(>=3)且最大砖块较大(>=9)，考虑8砖块 (2^3)
                double total_prob = prob_2 + prob_4;
                double weighted_score = score2 * prob_2 + score4 * prob_4;
                
                if (maxrank >= 9 && state->curdepth < 2) {
                    // 在符合条件的情况下偶尔考虑8
                    double prob_8 = 0.1;
                    double score8 = score_move_node(state, board | (3ULL << (pos * 4)), cprob * prob_8);
                    total_prob += prob_8;
                    weighted_score += score8 * prob_8;
                }
                
                // 归一化概率
                res += weighted_score / total_prob;
                sample_count++;
            }
        }
    }
    
    // 规范化结果 - 现在根据实际采样数量来规范化
    if (sample_count > 0) {
        res /= sample_count;
    }
    
    // 缓存结果
    if (state->curdepth < CACHE_DEPTH_LIMIT) {
        insert_to_table(state->trans_table, board, state->curdepth, res);
    }
    
    return res;
}

double score_move_node(EvalState *state, uint64_t board, double cprob) {
    if (state->curdepth >= state->depth_limit) {
        state->maxdepth = max(state->maxdepth, state->curdepth);
        return score_heur_board(board);
    }

    state->curdepth++;
    double best = 0.0;
    
    // 移动方向优先级：LEFT, UP, RIGHT, DOWN
    int move_order[4] = {LEFT, UP, RIGHT, DOWN};
    
    // 修改：在所有深度上评估全部四个方向
    int move_count = 4;  // 始终评估所有四个方向
    
    for (int i = 0; i < move_count; i++) {
        int move = move_order[i];
        uint64_t newboard = execute_move(move, board);
        state->moves_evaled++;

        if (board != newboard) {
            double score = score_tilechoose_node(state, newboard, cprob);
            if (score > best) {
                best = score;
            }
        }
    }

    state->curdepth--;
    return best;
}

double score_toplevel_move(EvalState *state, uint64_t board, int move) {
    uint64_t newboard = execute_move(move, board);
    
    if (board == newboard)
        return 0;
        
    return score_tilechoose_node(state, newboard, 1.0) + 1e-6;
}

int find_best_move(GameState* state, int depth_limit) {
    uint64_t board = state->board;
    EvalState eval_state;
    double best_score = 0;
    int best_move = -1;
    
    if (is_game_over(state)) {
        return -1;
    }
    
    // 限制最大搜索深度为15
    if (depth_limit > 15) {
        depth_limit = 15;
        printf("搜索深度已限制为15\n");
    }
    
    // 根据棋盘空位和最大砖块动态调整深度
    int empty_count = count_empty(board);
    int max_rank = get_max_rank(board);
    
    // 更加智能的深度调整策略
    if (empty_count < 4) {
        // 棋盘几乎满了，用较深的搜索
        depth_limit = min(depth_limit, max_rank >= 10 ? 7 : 6);
    } else if (empty_count < 7) {
        // 棋盘有一些空位，中等深度
        depth_limit = min(depth_limit, max_rank >= 9 ? 6 : 5);
    } else if (empty_count < 10) {
        // 空位较多，适中深度
        depth_limit = min(depth_limit, max_rank >= 9 ? 5 : 4);
    } else {
        // 空位很多，降低深度以加快速度
        depth_limit = min(depth_limit, 4);
    }
    
    // 临界决策点（即将取得重大进展时）增加深度
    int max_tile_value = 1 << max_rank;
    if (max_tile_value >= 512 && max_tile_value < 1024) {
        // 快要合成1024，增加深度
        depth_limit = min(depth_limit + 1, 7);
    }
    
    // 创建适当大小的转置表
    eval_state.trans_table = create_trans_table(TRANSTABLE_SIZE); 
    eval_state.maxdepth = 0;
    eval_state.curdepth = 0;
    eval_state.cachehits = 0;
    eval_state.moves_evaled = 0;
    eval_state.depth_limit = depth_limit;

    printf("AI思考中...(深度: %d, 空位: %d, 全方向搜索, 高密度采样)\n", depth_limit, empty_count);
    
    // 评估所有四个方向
    int move_order[4] = {LEFT, UP, RIGHT, DOWN};
    
    for (int i = 0; i < 4; i++) {
        int move = move_order[i];
        uint64_t newboard = execute_move(move, board);
        if (board != newboard) {
            double score = score_toplevel_move(&eval_state, board, move);
            if (score > best_score) {
                best_score = score;
                best_move = move;
            }
            
            printf("分析%s方向...得分: %.0f\n", 
                move == UP ? "UP" : (move == DOWN ? "DOWN" : (move == LEFT ? "LEFT" : "RIGHT")), 
                score);
        }
    }

    printf("AI评估了%d个位置，缓存命中%d次，最大深度%d\n", 
           eval_state.moves_evaled, eval_state.cachehits, eval_state.maxdepth);
    printf("最佳移动方向: %d, 得分: %.0f\n", best_move, best_score);

    // 清理转置表释放内存
    free_trans_table(eval_state.trans_table);
    return best_move;
}

// 检查棋盘上是否存在大于等于指定值的砖块
bool has_tile_gte(uint64_t board, int value) {
    int max_tile = 0;
    for (int i = 0; i < 16; i++) {
        int shift = i * 4;
        int tile_value = (board >> shift) & 0xf;
        if (tile_value > 0) {
            int actual_value = 1 << tile_value;
            if (actual_value >= value) {
                return true;
            }
            if (actual_value > max_tile) {
                max_tile = actual_value;
            }
        }
    }
    printf("棋盘最大值: %d\n", max_tile);
    return false;
}

// 添加随机砖块
uint64_t add_random_tile(uint64_t board) {
    int empty = count_empty(board);
    if (empty == 0) {
        printf("棋盘已满，无法添加新砖块\n");
        return board;
    }

    printf("添加新砖块，当前空位数: %d\n", empty);
    int index = rand() % empty;
    uint64_t tmp = board;
    
    for (int i = 0; i < 16; i++) {
        int shift = i * 4;
        uint64_t mask = (uint64_t)0xfULL << shift;
        if ((tmp & mask) == 0) {
            if (index == 0) {
                // 根据概率添加不同的砖块
                double prob = (double)rand() / RAND_MAX;
                unsigned tile_value;
                
                // 检查棋盘最大值
                int max_rank = get_max_rank(board);
                int max_tile = 1 << max_rank;
                
                // 根据最大砖块值调整生成概率分布
                if (max_tile >= 1024) {
                    // 当最大砖块≥1024时: 2(54%), 4(30%), 8(10%), 16(3%), 32(3%)
                    if (prob < 0.54) {
                        tile_value = 1; // 2^1 = 2
                    } else if (prob < 0.54 + 0.3) {
                        tile_value = 2; // 2^2 = 4
                    } else if (prob < 0.54 + 0.3 + 0.1) {
                        tile_value = 3; // 2^3 = 8
                    } else if (prob < 0.54 + 0.3 + 0.1 + 0.03) {
                        tile_value = 4; // 2^4 = 16
                    } else {
                        tile_value = 5; // 2^5 = 32
                    }
                    printf("使用高级概率分布: 2(54%%), 4(30%%), 8(10%%), 16(3%%), 32(3%%)\n");
                }
                else if (max_tile >= 512) {
                    // 当最大砖块≥512时: 2(57%), 4(30%), 8(10%), 16(3%)
                    if (prob < 0.57) {
                        tile_value = 1; // 2^1 = 2
                    } else if (prob < 0.57 + 0.3) {
                        tile_value = 2; // 2^2 = 4
                    } else if (prob < 0.57 + 0.3 + 0.1) {
                        tile_value = 3; // 2^3 = 8
                    } else {
                        tile_value = 4; // 2^4 = 16
                    }
                    printf("使用中级概率分布: 2(57%%), 4(30%%), 8(10%%), 16(3%%)\n");
                }
                else {
                    // 默认情况: 2(60%), 4(30%), 8(10%)
                    if (prob < 0.6) {
                        tile_value = 1; // 2^1 = 2
                    } else if (prob < 0.6 + 0.3) {
                        tile_value = 2; // 2^2 = 4
                    } else {
                        tile_value = 3; // 2^3 = 8
                    }
                    printf("使用基础概率分布: 2(60%%), 4(30%%), 8(10%%)\n");
                }
                
                printf("在位置 %d 添加砖块，值: %u (2^%u)\n", i, 1<<tile_value, tile_value);
                return board | ((uint64_t)tile_value << shift);
            }
            index--;
        }
    }
    
    printf("错误：未能添加新砖块\n");
    return board;
}

// 将棋盘转换为二维网格（用于显示）
void board_to_grid(uint64_t board, int grid[BOARD_SIZE][BOARD_SIZE]) {
    printf("转换棋盘到网格，棋盘值: %llu\n", board);
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            int shift = (i * BOARD_SIZE + j) * 4;
            int value = (board >> shift) & 0xf;
            grid[i][j] = value > 0 ? 1 << value : 0;
            if (value > 0) {
                printf("位置[%d][%d]: 砖块值 = %d (2^%d)\n", i, j, grid[i][j], value);
            }
        }
    }
}

// 将二维网格转换为棋盘
uint64_t grid_to_board(int grid[BOARD_SIZE][BOARD_SIZE]) {
    uint64_t board = 0;
    
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            int value = grid[i][j];
            int rank = 0;
            
            if (value > 0) {
                rank = (int)(log2(value));
            }
            
            int shift = (i * BOARD_SIZE + j) * 4;
            board |= (uint64_t)rank << shift;
        }
    }
    
    return board;
}

// 初始化游戏状态
void init_game(GameState* state) {
    // 重置所有状态
    state->board = 0;
    state->score = 0;
    state->game_over = false;
    
    // 添加初始的两个砖块
    state->board = add_random_tile(state->board);
    printf("添加第一个砖块后的棋盘状态: %llu\n", state->board);
    state->board = add_random_tile(state->board);
    printf("添加第二个砖块后的棋盘状态: %llu\n", state->board);
    
    // 如果还是空棋盘，强制添加两个砖块
    if (state->board == 0) {
        printf("强制添加砖块，因为自动随机添加失败\n");
        // 在左上角放置一个2
        state->board |= ((uint64_t)1 << 0);
        // 在右下角放置一个4
        state->board |= ((uint64_t)2 << 60);
        printf("强制添加后的棋盘状态: %llu\n", state->board);
    }
    
    // 检查是否成功初始化
    int empty_count = count_empty(state->board);
    printf("游戏初始化完成，棋盘状态: %llu，空白格子数: %d\n", state->board, empty_count);
    
    // 显示初始砖块位置
    int grid[BOARD_SIZE][BOARD_SIZE];
    board_to_grid(state->board, grid);
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (grid[i][j] > 0) {
                printf("初始砖块位置[%d][%d]值为: %d\n", i, j, grid[i][j]);
            }
        }
    }
}

// 判断游戏是否结束
bool is_game_over(GameState* state) {
    // 如果有空位，游戏未结束
    int empty_count = count_empty(state->board);
    if (empty_count > 0) {
        printf("棋盘还有 %d 个空位，游戏未结束\n", empty_count);
        return false;
    }
    
    printf("棋盘已满，检查是否有可合并的砖块...\n");
    // 检查是否有可能的移动
    for (int move = 0; move < 4; move++) {
        uint64_t new_board = execute_move(move, state->board);
        if (new_board != state->board) {
            printf("有可合并的砖块，游戏未结束\n");
            return false;
        }
    }
    
    printf("没有可合并的砖块，游戏结束\n");
    return true;
}

// 更新游戏分数
int calculate_move_score(uint64_t old_board, uint64_t new_board) {
    int score = 0;
    
    for (int i = 0; i < 16; i++) {
        int shift = i * 4;
        int old_value = (old_board >> shift) & 0xf;
        int new_value = (new_board >> shift) & 0xf;
        
        if (old_value != new_value && new_value > 0) {
            // 如果是通过合并产生的新值
            if (new_value > old_value) {
                score += (1 << new_value);
            }
        }
    }
    
    return score;
}

// 执行上移
bool move_up(GameState* state) {
    uint64_t new_board = execute_move(UP, state->board);
    
    if (new_board != state->board) {
        int move_score = calculate_move_score(state->board, new_board);
        state->board = new_board;
        state->score += move_score;
        
        // 更新最高分
        if (state->score > state->best_score) {
            state->best_score = state->score;
        }
        
        return true;
    }
    
    return false;
}

// 执行下移
bool move_down(GameState* state) {
    uint64_t new_board = execute_move(DOWN, state->board);
    
    if (new_board != state->board) {
        int move_score = calculate_move_score(state->board, new_board);
        state->board = new_board;
        state->score += move_score;
        
        // 更新最高分
        if (state->score > state->best_score) {
            state->best_score = state->score;
        }
        
        return true;
    }
    
    return false;
}

// 执行左移
bool move_left(GameState* state) {
    uint64_t new_board = execute_move(LEFT, state->board);
    
    if (new_board != state->board) {
        int move_score = calculate_move_score(state->board, new_board);
        state->board = new_board;
        state->score += move_score;
        
        // 更新最高分
        if (state->score > state->best_score) {
            state->best_score = state->score;
        }
        
        return true;
    }
    
    return false;
}

// 执行右移
bool move_right(GameState* state) {
    uint64_t new_board = execute_move(RIGHT, state->board);
    
    if (new_board != state->board) {
        int move_score = calculate_move_score(state->board, new_board);
        state->board = new_board;
        state->score += move_score;
        
        // 更新最高分
        if (state->score > state->best_score) {
            state->best_score = state->score;
        }
        
        return true;
    }
    
    return false;
} 