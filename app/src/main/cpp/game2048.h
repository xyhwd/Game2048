// game2048.h - 2048游戏主头文件
#ifndef GAME2048_H
#define GAME2048_H

#include <stdint.h>
#include <stdbool.h>

// 游戏常量
#define BOARD_SIZE 4
#define ROW_MASK 0xFFFF
#define ROW_MAX 65536  // 2^16
#define MAX_RANK 11     // 2^11=2048 作为最大可合并数字

// 游戏方向
#define UP 0
#define DOWN 1
#define LEFT 2
#define RIGHT 3

// 砖块数值和评分参数
#define SCORE_LOST_PENALTY 200000.0
#define SCORE_MONOTONICITY_POWER 4.0
#define SCORE_MONOTONICITY_WEIGHT 47.0
#define SCORE_SUM_POWER 3.5
#define SCORE_SUM_WEIGHT 11.0
#define SCORE_MERGES_WEIGHT 700.0
#define SCORE_EMPTY_WEIGHT 270.0
#define CPROB_THRESH_BASE 0.0001
#define CACHE_DEPTH_LIMIT 20

// 新砖块生成概率
#define TILE_2_PROB 0.5
#define TILE_4_PROB 0.3
#define TILE_8_PROB 0.1
#define TILE_16_PROB 0.05
#define TILE_32_PROB 0.05

// 转置表大小定义
#define TRANSTABLE_SIZE 10485760  // 增加转置表大小，约为10M条目

// 游戏状态结构体
typedef struct {
    uint64_t board;             // 64位整数表示的4x4棋盘
    int score;                  // 当前分数
    int best_score;             // 最高分
    bool game_over;             // 游戏是否结束
} GameState;

// 评估状态结构体
typedef struct {
    void* trans_table;          // 转置表（C版本使用哈希表）
    int maxdepth;               // 最大搜索深度
    int curdepth;               // 当前搜索深度
    int cachehits;              // 缓存命中次数
    int moves_evaled;           // 评估的移动数
    int depth_limit;            // 深度限制
} EvalState;

// 核心游戏函数声明
void init_tables(void);
uint64_t execute_move(int move, uint64_t board);
int count_empty(uint64_t board);
uint64_t transpose(uint64_t board);
uint64_t add_random_tile(uint64_t board);
int get_max_rank(uint64_t board);

// 得分函数
double score_board(uint64_t board);
double score_heur_board(uint64_t board);

// AI算法函数
int find_best_move(GameState* state, int depth_limit);

// 辅助函数
void board_to_grid(uint64_t board, int grid[BOARD_SIZE][BOARD_SIZE]);
uint64_t grid_to_board(int grid[BOARD_SIZE][BOARD_SIZE]);
bool is_game_over(GameState* state);

// 游戏操作函数
void init_game(GameState* state);
bool move_up(GameState* state);
bool move_down(GameState* state);
bool move_left(GameState* state);
bool move_right(GameState* state);

#endif // GAME2048_H 