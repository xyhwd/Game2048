package com.example.game2048;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.Typeface;
import android.util.AttributeSet;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.View;

/**
 * 2048游戏视图
 * 负责游戏绘制和触摸控制
 */
public class GameView extends View {

    // 游戏逻辑对象
    private Game2048 game;
    
    // 手势检测器
    private GestureDetector gestureDetector;
    
    // 绘制相关
    private Paint backgroundPaint;
    private Paint cellPaint;
    private Paint textPaint;
    
    // 游戏网格
    private int[][] grid;
    
    // 网格单元大小和间距
    private float cellSize;
    private float cellMargin;
    
    // 砖块颜色映射
    private static final int[] TILE_COLORS = {
        0xffCDC1B4, // 空白
        0xffeee4da, // 2
        0xffede0c8, // 4
        0xfff2b179, // 8
        0xfff59563, // 16
        0xfff67c5f, // 32
        0xfff65e3b, // 64
        0xffedcf72, // 128
        0xffedcc61, // 256
        0xffedc850, // 512
        0xffedc53f, // 1024
        0xffedc22e, // 2048
        0xff3c3a32, // 4096+
    };
    
    // 游戏监听接口
    public interface GameListener {
        void onScoreChanged(int score, int bestScore);
        void onGameOver();
    }
    
    private GameListener gameListener;
    
    public GameView(Context context) {
        super(context);
        init();
    }
    
    public GameView(Context context, AttributeSet attrs) {
        super(context, attrs);
        init();
    }
    
    public GameView(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init();
    }
    
    private void init() {
        game = new Game2048();
        
        // 初始化画笔
        backgroundPaint = new Paint();
        backgroundPaint.setColor(0xffbbada0); // 背景色
        
        cellPaint = new Paint();
        
        textPaint = new Paint();
        textPaint.setAntiAlias(true);
        textPaint.setTextAlign(Paint.Align.CENTER);
        
        // 初始化手势检测
        gestureDetector = new GestureDetector(getContext(), new GestureListener());
        
        // 初始化游戏
        game.initGame();
        updateGrid();
    }
    
    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        
        // 保持方形布局
        int size = Math.min(getMeasuredWidth(), getMeasuredHeight());
        setMeasuredDimension(size, size);
        
        // 计算单元格大小和边距
        float viewSize = size - getPaddingLeft() - getPaddingRight();
        cellMargin = viewSize / 50; // 边距为总大小的2%
        cellSize = (viewSize - cellMargin * 5) / 4; // 4个格子，5个边距
    }
    
    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        
        // 绘制背景
        canvas.drawRect(0, 0, getWidth(), getHeight(), backgroundPaint);
        
        float startX = getPaddingLeft() + cellMargin;
        float startY = getPaddingTop() + cellMargin;
        
        // 绘制每个砖块
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                int value = grid[i][j];
                
                // 计算位置
                float left = startX + j * (cellSize + cellMargin);
                float top = startY + i * (cellSize + cellMargin);
                float right = left + cellSize;
                float bottom = top + cellSize;
                
                // 确定颜色
                int colorIndex = 0;
                if (value > 0) {
                    colorIndex = (int)(Math.log(value) / Math.log(2));
                    if (colorIndex >= TILE_COLORS.length) {
                        colorIndex = TILE_COLORS.length - 1;
                    }
                }
                
                // 绘制砖块背景
                cellPaint.setColor(TILE_COLORS[colorIndex]);
                RectF rect = new RectF(left, top, right, bottom);
                canvas.drawRoundRect(rect, cellSize / 10, cellSize / 10, cellPaint);
                
                // 如果不是空白，绘制数字
                if (value > 0) {
                    // 数字颜色
                    textPaint.setColor(value <= 4 ? 0xff776e65 : 0xfff9f6f2);
                    
                    // 设置字体大小（根据数字位数调整）
                    float textSize = cellSize / (value < 100 ? 2 : value < 1000 ? 2.5f : 3);
                    textPaint.setTextSize(textSize);
                    textPaint.setTypeface(Typeface.DEFAULT_BOLD);
                    
                    // 计算文本位置
                    Paint.FontMetrics fm = textPaint.getFontMetrics();
                    float textHeight = fm.bottom - fm.top;
                    float textBaseY = (top + bottom - textHeight) / 2 - fm.top;
                    
                    // 绘制数字
                    canvas.drawText(String.valueOf(value), (left + right) / 2, textBaseY, textPaint);
                }
            }
        }
    }
    
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        return gestureDetector.onTouchEvent(event);
    }
    
    // 更新网格数据
    private void updateGrid() {
        grid = game.getBoardGrid();
        
        // 通知分数变化
        if (gameListener != null) {
            gameListener.onScoreChanged(game.getScore(), game.getBestScore());
            
            // 检查游戏是否结束
            if (game.isGameOver()) {
                gameListener.onGameOver();
            }
        }
        
        invalidate();
    }
    
    // 设置游戏监听器
    public void setGameListener(GameListener listener) {
        this.gameListener = listener;
    }
    
    // 重新开始游戏
    public void restart() {
        game.initGame();
        updateGrid();
    }
    
    // 使用AI移动
    public void moveAI() {
        int direction = game.getAIMove(3); // 深度为3
        if (direction >= 0) {
            game.move(direction);
            updateGrid();
        }
    }
    
    // 手势监听类
    private class GestureListener extends GestureDetector.SimpleOnGestureListener {
        
        private static final int SWIPE_THRESHOLD = 100;
        private static final int SWIPE_VELOCITY_THRESHOLD = 100;
        
        @Override
        public boolean onDown(MotionEvent e) {
            return true;
        }
        
        @Override
        public boolean onFling(MotionEvent e1, MotionEvent e2, float velocityX, float velocityY) {
            boolean result = false;
            
            try {
                float diffY = e2.getY() - e1.getY();
                float diffX = e2.getX() - e1.getX();
                
                // 通过X和Y方向的移动距离判断移动方向
                if (Math.abs(diffX) > Math.abs(diffY)) {
                    // 水平移动更明显
                    if (Math.abs(diffX) > SWIPE_THRESHOLD && Math.abs(velocityX) > SWIPE_VELOCITY_THRESHOLD) {
                        if (diffX > 0) {
                            // 向右滑动
                            result = game.move(Game2048.RIGHT);
                        } else {
                            // 向左滑动
                            result = game.move(Game2048.LEFT);
                        }
                    }
                } else {
                    // 垂直移动更明显
                    if (Math.abs(diffY) > SWIPE_THRESHOLD && Math.abs(velocityY) > SWIPE_VELOCITY_THRESHOLD) {
                        if (diffY > 0) {
                            // 向下滑动
                            result = game.move(Game2048.DOWN);
                        } else {
                            // 向上滑动
                            result = game.move(Game2048.UP);
                        }
                    }
                }
                
                // 如果移动有效，更新网格
                if (result) {
                    updateGrid();
                }
                
            } catch (Exception e) {
                e.printStackTrace();
            }
            
            return result;
        }
    }
} 