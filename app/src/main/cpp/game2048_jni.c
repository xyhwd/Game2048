#include <jni.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "game2048.h"

// 全局游戏状态对象
static GameState gameState;
static bool is_initialized = false;

// 初始化游戏
JNIEXPORT void JNICALL
Java_com_example_game2048_Game2048_initGame(JNIEnv *env, jobject thisObj) {
    if (!is_initialized) {
        // 初始化随机数种子
        srand(time(NULL));
        // 初始化游戏表格
        init_tables();
        is_initialized = true;
    }
    
    // 初始化游戏状态
    init_game(&gameState);
}

// 获取棋盘状态
JNIEXPORT jintArray JNICALL
Java_com_example_game2048_Game2048_getBoard(JNIEnv *env, jobject thisObj) {
    int grid[BOARD_SIZE][BOARD_SIZE];
    board_to_grid(gameState.board, grid);
    
    // 创建一维数组返回给Java
    jintArray result = (*env)->NewIntArray(env, BOARD_SIZE * BOARD_SIZE);
    if (result == NULL) {
        return NULL; // 内存不足
    }
    
    // 将二维网格转换为一维数组
    jint buffer[BOARD_SIZE * BOARD_SIZE];
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            buffer[i * BOARD_SIZE + j] = grid[i][j];
        }
    }
    
    // 设置数组内容
    (*env)->SetIntArrayRegion(env, result, 0, BOARD_SIZE * BOARD_SIZE, buffer);
    
    return result;
}

// 执行移动
JNIEXPORT jboolean JNICALL
Java_com_example_game2048_Game2048_move(JNIEnv *env, jobject thisObj, jint direction) {
    bool moved = false;
    
    switch (direction) {
        case UP:
            moved = move_up(&gameState);
            break;
        case DOWN:
            moved = move_down(&gameState);
            break;
        case LEFT:
            moved = move_left(&gameState);
            break;
        case RIGHT:
            moved = move_right(&gameState);
            break;
        default:
            return JNI_FALSE;
    }
    
    return moved ? JNI_TRUE : JNI_FALSE;
}

// 获取当前分数
JNIEXPORT jint JNICALL
Java_com_example_game2048_Game2048_getScore(JNIEnv *env, jobject thisObj) {
    return gameState.score;
}

// 获取最高分数
JNIEXPORT jint JNICALL
Java_com_example_game2048_Game2048_getBestScore(JNIEnv *env, jobject thisObj) {
    return gameState.best_score;
}

// 检查游戏是否结束
JNIEXPORT jboolean JNICALL
Java_com_example_game2048_Game2048_isGameOver(JNIEnv *env, jobject thisObj) {
    return is_game_over(&gameState) ? JNI_TRUE : JNI_FALSE;
}

// AI移动
JNIEXPORT jint JNICALL
Java_com_example_game2048_Game2048_getAIMove(JNIEnv *env, jobject thisObj, jint depth) {
    return find_best_move(&gameState, depth);
} 