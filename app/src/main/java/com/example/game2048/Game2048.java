package com.example.game2048;

/**
 * 2048游戏JNI接口类
 * 封装了与C代码的交互
 */
public class Game2048 {
    // 加载native库
    static {
        System.loadLibrary("game2048");
    }
    
    // 游戏方向常量
    public static final int UP = 0;
    public static final int DOWN = 1;
    public static final int LEFT = 2;
    public static final int RIGHT = 3;
    
    // 棋盘大小
    public static final int BOARD_SIZE = 4;
    
    // 初始化游戏
    public native void initGame();
    
    // 获取棋盘状态
    public native int[] getBoard();
    
    // 移动操作
    public native boolean move(int direction);
    
    // 获取当前分数
    public native int getScore();
    
    // 获取最高分数
    public native int getBestScore();
    
    // 检查游戏是否结束
    public native boolean isGameOver();
    
    // 获取AI建议的移动
    public native int getAIMove(int depth);
    
    // 辅助方法：将一维数组转为二维
    public int[][] getBoardGrid() {
        int[] flat = getBoard();
        int[][] grid = new int[BOARD_SIZE][BOARD_SIZE];
        
        for (int i = 0; i < BOARD_SIZE; i++) {
            for (int j = 0; j < BOARD_SIZE; j++) {
                grid[i][j] = flat[i * BOARD_SIZE + j];
            }
        }
        
        return grid;
    }
    
    // 打印棋盘状态（调试用）
    public String getBoardString() {
        int[][] grid = getBoardGrid();
        StringBuilder sb = new StringBuilder();
        
        for (int i = 0; i < BOARD_SIZE; i++) {
            for (int j = 0; j < BOARD_SIZE; j++) {
                sb.append(String.format("%4d ", grid[i][j]));
            }
            sb.append("\n");
        }
        
        return sb.toString();
    }
} 