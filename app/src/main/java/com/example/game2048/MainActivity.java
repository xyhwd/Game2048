package com.example.game2048;

import android.app.AlertDialog;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;
import androidx.appcompat.app.AppCompatActivity;

public class MainActivity extends AppCompatActivity implements GameView.GameListener {

    private GameView gameView;
    private TextView scoreTextView;
    private TextView bestScoreTextView;
    private Button restartButton;
    private Button aiHintButton;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        // 初始化视图
        gameView = findViewById(R.id.game_view);
        scoreTextView = findViewById(R.id.score_text);
        bestScoreTextView = findViewById(R.id.best_score_text);
        restartButton = findViewById(R.id.restart_button);
        aiHintButton = findViewById(R.id.ai_hint_button);

        // 设置游戏监听器
        gameView.setGameListener(this);

        // 设置按钮监听器
        restartButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                gameView.restart();
            }
        });

        aiHintButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                gameView.moveAI();
            }
        });
    }

    @Override
    public void onScoreChanged(int score, int bestScore) {
        // 更新分数显示
        scoreTextView.setText(String.valueOf(score));
        bestScoreTextView.setText(String.valueOf(bestScore));
    }

    @Override
    public void onGameOver() {
        // 显示游戏结束对话框
        new AlertDialog.Builder(this)
            .setTitle(R.string.game_over)
            .setMessage(getString(R.string.game_over_message, scoreTextView.getText()))
            .setPositiveButton(R.string.restart, (dialog, which) -> gameView.restart())
            .setNegativeButton(R.string.close, null)
            .show();
    }
} 