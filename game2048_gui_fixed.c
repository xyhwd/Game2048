// game2048_gui.c - 游戏图形界面
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <SDL.h>
#include <SDL_ttf.h>
#include "game2048.h"
#include <sys/stat.h> // 添加文件检查需要的头文件

// 外部函数声明
extern void set_status_text(const char* text);
extern void get_ai_hint(void);
extern void auto_play_move(void);
extern void handle_key_input(SDL_KeyboardEvent* key);
extern void handle_mouse_click(int x, int y);

// 按钮定义
typedef struct {
    SDL_Rect rect;
    char* text;
    bool hover;
} Button;

// 常量定义
#define WINDOW_WIDTH 600
#define WINDOW_HEIGHT 750  // 增加窗口高度，原来是700
#define GRID_SIZE 4
#define TILE_SIZE 110
#define TILE_MARGIN 5
#define GRID_PADDING 5
#define GRID_WIDTH ((TILE_SIZE + TILE_MARGIN) * GRID_SIZE + GRID_PADDING)
#define GRID_HEIGHT GRID_WIDTH
#define GRID_OFFSET_X ((WINDOW_WIDTH - GRID_WIDTH) / 2)
#define GRID_OFFSET_Y 200  // 增加网格Y偏移，为AI深度按钮留出更多空间，原来是150

// 颜色定义
typedef struct {
    Uint8 r, g, b;
} Color;

typedef struct {
    Color bg;
    Color fg;
} TileColor;

// 砖块颜色表
static TileColor tile_colors[17] = {
    // 0 - 空砖块
    {{0xcd, 0xc1, 0xb4}, {0x77, 0x6e, 0x65}},
    // 2
    {{0xee, 0xe4, 0xda}, {0x77, 0x6e, 0x65}},
    // 4
    {{0xed, 0xe0, 0xc8}, {0x77, 0x6e, 0x65}},
    // 8
    {{0xf2, 0xb1, 0x79}, {0xf9, 0xf6, 0xf2}},
    // 16
    {{0xf5, 0x95, 0x63}, {0xf9, 0xf6, 0xf2}},
    // 32
    {{0xf6, 0x7c, 0x5f}, {0xf9, 0xf6, 0xf2}},
    // 64
    {{0xf6, 0x5e, 0x3b}, {0xf9, 0xf6, 0xf2}},
    // 128
    {{0xed, 0xcf, 0x72}, {0xf9, 0xf6, 0xf2}},
    // 256
    {{0xed, 0xcc, 0x61}, {0xf9, 0xf6, 0xf2}},
    // 512
    {{0xed, 0xc8, 0x50}, {0xf9, 0xf6, 0xf2}},
    // 1024
    {{0xed, 0xc5, 0x3f}, {0xf9, 0xf6, 0xf2}},
    // 2048
    {{0xed, 0xc2, 0x2e}, {0xf9, 0xf6, 0xf2}},
    // 4096
    {{0xed, 0x70, 0x2e}, {0xf9, 0xf6, 0xf2}},
    // 8192
    {{0xed, 0x4c, 0x2e}, {0xf9, 0xf6, 0xf2}},
    // 16384
    {{0xed, 0x2e, 0x8f}, {0xf9, 0xf6, 0xf2}},
    // 32768
    {{0xed, 0x2e, 0x5b}, {0xf9, 0xf6, 0xf2}},
    // 65536及以上
    {{0xed, 0x2e, 0x2e}, {0xf9, 0xf6, 0xf2}}
};

// 砖块图片纹理
SDL_Texture* tile_textures[12] = {NULL}; // 索引0不使用，1-11对应1.png到11.png

// 全局变量
SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
TTF_Font* font_large = NULL;
TTF_Font* font_medium = NULL;
TTF_Font* font_small = NULL;
GameState game_state;
GameState history[10];  // 历史记录，用于撤销
int history_count = 0;
bool auto_play = false;
int ai_depth = 5;
// 全局状态文本
char status_text[100] = "使用方向键或WASD控制";
Uint32 status_time = 0;

// 自定义模式相关变量
bool custom_mode = false;
int selected_row = 0;
int selected_col = 0;
int selected_value = 2; // 默认值为2

// 按钮
Button new_game_btn = {{20, 90, 120, 40}, "新游戏", false};
Button undo_btn = {{160, 90, 120, 40}, "撤销", false};
Button hint_btn = {{300, 90, 120, 40}, "AI提示", false};
Button auto_play_btn = {{440, 90, 140, 40}, "AI自动游戏", false};
// 添加自定义模式按钮
Button custom_mode_btn = {{20, 50, 120, 40}, "自定义模式", false};
Button apply_custom_btn = {{160, 50, 120, 40}, "应用", false};
// 砖块值调整按钮
Button increment_btn = {{300, 50, 40, 40}, "+", false};
Button decrement_btn = {{350, 50, 40, 40}, "-", false};
// AI深度控制按钮
Button ai_depth_inc_btn = {{250, 140, 40, 30}, "+", false};
Button ai_depth_dec_btn = {{170, 140, 40, 30}, "-", false};
// 使标题颜色成为全局变量，在render_game中使用
SDL_Color titleColor = {0x77, 0x6E, 0x65, 0xFF};

// 检查文件是否存在
bool file_exists(const char* filename) {
    struct stat buffer;
    return (stat(filename, &buffer) == 0);
}

// 获取砖块颜色索引
int get_color_index(int value) {
    if (value == 0) return 0;
    
    int index = (int)(log2(value));
    if (index >= 16) index = 16;  // 最大索引
    
    return index;
}

// 生成砖块图片
SDL_Surface* generate_tile_surface(int value) {
    // 计算2的幂次
    int power = 0;
    if (value > 0) {
        power = log2(value);
    }
    
    // 获取砖块颜色
    int color_index = get_color_index(value);
    Color bg_color = tile_colors[color_index].bg;
    Color fg_color = tile_colors[color_index].fg;
    
    // 创建表面 - 使用更通用的格式
    SDL_Surface* surface = SDL_CreateRGBSurface(0, TILE_SIZE, TILE_SIZE, 32, 
                                             0, 0, 0, 0);
    if (!surface) {
        fprintf(stderr, "无法创建表面: %s\n", SDL_GetError());
        return NULL;
    }
    
    // 设置背景色
    SDL_FillRect(surface, NULL, 
                SDL_MapRGB(surface->format, bg_color.r, bg_color.g, bg_color.b));
    
    // 如果不是空砖块，添加文本
    if (value > 0) {
        // 根据数字位数选择字体大小
        TTF_Font* font = font_large;
        if (value >= 1000) {
            font = font_small;
        } else if (value >= 100) {
            font = font_medium;
        }
        
        // 渲染文本 - 显示幂值(1-11)而不是原始值(2-2048)
        char text[10];
        sprintf(text, "%d", power);  // 直接使用幂次值
        
        SDL_Color text_color = {fg_color.r, fg_color.g, fg_color.b, 255};
        SDL_Surface* text_surface = TTF_RenderText_Blended(font, text, text_color);
        
        if (text_surface) {
            // 将文本居中放置在砖块上
            SDL_Rect dest_rect = {
                (TILE_SIZE - text_surface->w) / 2,
                (TILE_SIZE - text_surface->h) / 2,
                text_surface->w,
                text_surface->h
            };
            
            // 复制文本到砖块表面
            SDL_BlitSurface(text_surface, NULL, surface, &dest_rect);
            SDL_FreeSurface(text_surface);
        }
    }
    
    return surface;
}

// 添加加载BMP图片函数
bool load_tile_images() {
    char filename[100];
    bool any_loaded = false;
    
    printf("开始加载BMP图片...\n");
    
    // 尝试加载图片
    for (int i = 1; i <= 11; i++) {
        sprintf(filename, "E:/%d.bmp", i);
        
        printf("正在尝试加载图片: %s\n", filename);
        
        if (file_exists(filename)) {
            printf("文件存在: %s\n", filename);
            SDL_Surface* surface = SDL_LoadBMP(filename);
            
            if (surface) {
                printf("成功加载图片到表面，宽度=%d, 高度=%d\n", surface->w, surface->h);
                
                // 创建纹理
                tile_textures[i] = SDL_CreateTextureFromSurface(renderer, surface);
                if (tile_textures[i]) {
                    printf("已加载图片成功并创建纹理: %s\n", filename);
                    any_loaded = true;
                } else {
                    printf("创建纹理失败: %s\n", SDL_GetError());
                }
                SDL_FreeSurface(surface);
            } else {
                fprintf(stderr, "无法加载图片 %s: %s\n", filename, SDL_GetError());
            }
        } else {
            fprintf(stderr, "图片文件不存在: %s\n", filename);
        }
    }
    
    if (any_loaded) {
        printf("图片加载完成，成功加载%s使用BMP图片显示模式\n", any_loaded ? "" : "失败，");
        set_status_text("已加载BMP图片显示模式");
    } else {
        printf("未找到任何有效的BMP图片文件\n");
        set_status_text("未找到BMP图片文件，使用文本模式");
    }
    
    return true;
}

// 释放图片资源
void free_tile_images() {
    for (int i = 1; i <= 11; i++) {
        if (tile_textures[i]) {
            SDL_DestroyTexture(tile_textures[i]);
            tile_textures[i] = NULL;
        }
    }
}

// 初始化SDL
bool init_sdl() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL初始化失败: %s\n", SDL_GetError());
        return false;
    }
    window = SDL_CreateWindow("炼器手动挡",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN);
    if (!window) {
        fprintf(stderr, "无法创建窗口: %s\n", SDL_GetError());
        return false;
    }
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        fprintf(stderr, "无法创建渲染器: %s\n", SDL_GetError());
        return false;
    }
    // 初始化SDL_ttf
    if (TTF_Init() < 0) {
        fprintf(stderr, "SDL_ttf初始化失败: %s\n", TTF_GetError());
        return false;
    }
    // 加载字体
    font_large = TTF_OpenFont("simhei.ttf", 36);
    font_medium = TTF_OpenFont("simhei.ttf", 24);
    font_small = TTF_OpenFont("simhei.ttf", 18);
    if (!font_large || !font_medium || !font_small) {
        fprintf(stderr, "无法加载字体: %s\n", TTF_GetError());
        fprintf(stderr, "尝试使用系统默认字体...\n");
        // 尝试系统字体
        font_large = TTF_OpenFont("C:/Windows/Fonts/simhei.ttf", 36);
        font_medium = TTF_OpenFont("C:/Windows/Fonts/simhei.ttf", 24);
        font_small = TTF_OpenFont("C:/Windows/Fonts/simhei.ttf", 18);
        if (!font_large || !font_medium || !font_small) {
            fprintf(stderr, "无法加载系统字体: %s\n", TTF_GetError());
            return false;
        }
    }
    
    // 加载BMP图片
    load_tile_images();
    
    return true;
}

// 清理资源
void cleanup() {
    // 释放图片资源
    free_tile_images();
    
    if (font_large) TTF_CloseFont(font_large);
    if (font_medium) TTF_CloseFont(font_medium);
    if (font_small) TTF_CloseFont(font_small);
    TTF_Quit();
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
}

// 保存当前状态到历史记录
void save_state() {
    // 移动历史记录
    if (history_count >= 10) {
        for (int i = 0; i < 9; i++) {
            history[i] = history[i+1];
        }
        history_count = 9;
    }
    // 保存当前状态
    history[history_count] = game_state;
    history_count++;
}

// 撤销上一步
void undo_move() {
    if (history_count > 0) {
        history_count--;
        game_state = history[history_count];
    }
}

// 获取砖块显示文本（修改为显示幂值而非原始值）
const char* get_tile_text(int value) {
    static char buffer[10];
    if (value == 0) {
        return "";
    } else {
        // 计算幂值（log2(value)）并显示
        int power = (int)log2(value);
        sprintf(buffer, "%d", power);
        return buffer;
    }
}

// 渲染文本
SDL_Texture* render_text(const char* text, TTF_Font* font, SDL_Color color) {
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text, color);
    if (!surface) return NULL;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    return texture;
}

// 绘制按钮
void draw_button(Button* btn) {
    // 设置按钮背景色
    if (btn->hover) {
        SDL_SetRenderDrawColor(renderer, 0x80, 0x80, 0x80, 0xFF);
    } else {
        SDL_SetRenderDrawColor(renderer, 0xA0, 0xA0, 0xA0, 0xFF);
    }
    SDL_RenderFillRect(renderer, &btn->rect);
    // 设置按钮边框
    SDL_SetRenderDrawColor(renderer, 0x50, 0x50, 0x50, 0xFF);
    SDL_RenderDrawRect(renderer, &btn->rect);
    // 渲染按钮文本
    SDL_Color textColor = {0xFF, 0xFF, 0xFF, 0xFF};
    SDL_Texture* textTexture = render_text(btn->text, font_small, textColor);
    if (textTexture) {
        int text_width, text_height;
        SDL_QueryTexture(textTexture, NULL, NULL, &text_width, &text_height);
        SDL_Rect textRect = {
            btn->rect.x + (btn->rect.w - text_width) / 2,
            btn->rect.y + (btn->rect.h - text_height) / 2,
            text_width,
            text_height
        };
        SDL_RenderCopy(renderer, textTexture, NULL, &textRect);
        SDL_DestroyTexture(textTexture);
    }
}

// 检查按钮是否被点击
bool is_button_clicked(Button* btn, int x, int y) {
    return (x >= btn->rect.x && x < btn->rect.x + btn->rect.w &&
            y >= btn->rect.y && y < btn->rect.y + btn->rect.h);
}

// 更新按钮悬停状态
void update_button_hover(Button* btn, int x, int y) {
    btn->hover = is_button_clicked(btn, x, y);
}

// 绘制游戏界面
void render_game() {
    // 清除背景
    SDL_SetRenderDrawColor(renderer, 0xFA, 0xF8, 0xEF, 0xFF);
    SDL_RenderClear(renderer);

    // 显示调试信息
    char debugInfo[100];
    sprintf(debugInfo, "棋盘值: %llu, 空位: %d", game_state.board, count_empty(game_state.board));
    SDL_Texture* debugTexture = render_text(debugInfo, font_small, titleColor);
    if (debugTexture) {
        int text_width, text_height;
        SDL_QueryTexture(debugTexture, NULL, NULL, &text_width, &text_height);
        SDL_Rect debugRect = {20, WINDOW_HEIGHT - 60, text_width, text_height};
        SDL_RenderCopy(renderer, debugTexture, NULL, &debugRect);
        SDL_DestroyTexture(debugTexture);
    }

    // 绘制游戏标题
    SDL_Texture* titleTexture = render_text("2048", font_large, titleColor);
    if (titleTexture) {
        int text_width, text_height;
        SDL_QueryTexture(titleTexture, NULL, NULL, &text_width, &text_height);
        SDL_Rect titleRect = {20, 20, text_width, text_height};
        SDL_RenderCopy(renderer, titleTexture, NULL, &titleRect);
        SDL_DestroyTexture(titleTexture);
    }
    
    // 显示分数
    char scoreText[50];
    sprintf(scoreText, "分数: %d", game_state.score);
    SDL_Texture* scoreTexture = render_text(scoreText, font_medium, titleColor);
    if (scoreTexture) {
        int text_width, text_height;
        SDL_QueryTexture(scoreTexture, NULL, NULL, &text_width, &text_height);
        SDL_Rect scoreRect = {WINDOW_WIDTH - 150, 20, text_width, text_height};
        SDL_RenderCopy(renderer, scoreTexture, NULL, &scoreRect);
        SDL_DestroyTexture(scoreTexture);
    }
    
    // 显示最高分
    char bestScoreText[50];
    sprintf(bestScoreText, "最高分: %d", game_state.best_score);
    SDL_Texture* bestScoreTexture = render_text(bestScoreText, font_small, titleColor);
    if (bestScoreTexture) {
        int text_width, text_height;
        SDL_QueryTexture(bestScoreTexture, NULL, NULL, &text_width, &text_height);
        SDL_Rect bestScoreRect = {WINDOW_WIDTH - 150, 50, text_width, text_height};
        SDL_RenderCopy(renderer, bestScoreTexture, NULL, &bestScoreRect);
        SDL_DestroyTexture(bestScoreTexture);
    }
    
    // 绘制所有按钮
    draw_button(&new_game_btn);
    draw_button(&undo_btn);
    draw_button(&hint_btn);
    draw_button(&auto_play_btn);
    draw_button(&custom_mode_btn);
    draw_button(&apply_custom_btn);
    draw_button(&increment_btn);
    draw_button(&decrement_btn);
    draw_button(&ai_depth_inc_btn);
    draw_button(&ai_depth_dec_btn);
    
    // 显示自定义模式状态
    if (custom_mode) {
        char customText[50];
        int power = (selected_value > 0) ? (int)log2(selected_value) : 0;
        sprintf(customText, "自定义模式: 已选[%d,%d] 值:%d(2^%d)", selected_row, selected_col, selected_value, power);
        SDL_Texture* customTexture = render_text(customText, font_small, titleColor);
        if (customTexture) {
            int text_width, text_height;
            SDL_QueryTexture(customTexture, NULL, NULL, &text_width, &text_height);
            SDL_Rect customRect = {400, 50, text_width, text_height};
            SDL_RenderCopy(renderer, customTexture, NULL, &customRect);
            SDL_DestroyTexture(customTexture);
        }
    }
    
    // 显示自动游戏状态
    char autoPlayText[50];
    sprintf(autoPlayText, "AI深度: %d", ai_depth);
    SDL_Texture* autoPlayTexture = render_text(autoPlayText, font_small, titleColor);
    if (autoPlayTexture) {
        int text_width, text_height;
        SDL_QueryTexture(autoPlayTexture, NULL, NULL, &text_width, &text_height);
        SDL_Rect autoPlayRect = {20, 140, text_width, text_height};
        SDL_RenderCopy(renderer, autoPlayTexture, NULL, &autoPlayRect);
        SDL_DestroyTexture(autoPlayTexture);
    }
    
    // 绘制游戏网格背景
    SDL_SetRenderDrawColor(renderer, 0xBB, 0xAD, 0xA0, 0xFF);
    SDL_Rect gridRect = {GRID_OFFSET_X, GRID_OFFSET_Y, GRID_WIDTH, GRID_HEIGHT};
    SDL_RenderFillRect(renderer, &gridRect);
    
    // 绘制每个砖块
    int grid[BOARD_SIZE][BOARD_SIZE];
    // 将游戏板转换为网格
    board_to_grid(game_state.board, grid);
    for (int row = 0; row < GRID_SIZE; row++) {
        for (int col = 0; col < GRID_SIZE; col++) {
            int value = grid[row][col];
            int color_index = get_color_index(value);
            // 计算砖块位置
            SDL_Rect tileRect = {
                GRID_OFFSET_X + GRID_PADDING + col * (TILE_SIZE + TILE_MARGIN),
                GRID_OFFSET_Y + GRID_PADDING + row * (TILE_SIZE + TILE_MARGIN),
                TILE_SIZE,
                TILE_SIZE
            };
            
            // 设置砖块背景色
            SDL_SetRenderDrawColor(renderer, 
                tile_colors[color_index].bg.r,
                tile_colors[color_index].bg.g,
                tile_colors[color_index].bg.b,
                0xFF);
            SDL_RenderFillRect(renderer, &tileRect);
            
            // 如果在自定义模式下当前砖块被选中，绘制边框
            if (custom_mode && row == selected_row && col == selected_col) {
                SDL_SetRenderDrawColor(renderer, 0xFF, 0, 0, 0xFF); // 红色边框
                SDL_RenderDrawRect(renderer, &tileRect);
                // 加粗边框
                SDL_Rect innerRect = {
                    tileRect.x + 1, tileRect.y + 1,
                    tileRect.w - 2, tileRect.h - 2
                };
                SDL_RenderDrawRect(renderer, &innerRect);
            }
            
            // 如果不是空砖块，显示数字
            if (value > 0) {
                // 计算图片索引
                int img_index = (int)(log2(value));
                
                // 尝试使用图片渲染
                if (img_index >= 1 && img_index <= 11 && tile_textures[img_index]) {
                    // 固定使用索引对应的图片
                    SDL_RenderCopy(renderer, tile_textures[img_index], NULL, &tileRect);
                } else {
                    // 如果图片不可用，则使用文本
                    // 根据数字位数调整字体大小 - 显示幂次总是小于两位数，始终使用大字体
                    TTF_Font* tileFont = font_large;
                    // 设置文本颜色
                    SDL_Color textColor = {
                        tile_colors[color_index].fg.r,
                        tile_colors[color_index].fg.g,
                        tile_colors[color_index].fg.b,
                        0xFF
                    };
                    // 渲染砖块文本
                    const char* tileText = get_tile_text(value);
                    SDL_Texture* tileTexture = render_text(tileText, tileFont, textColor);
                    if (tileTexture) {
                        int text_width, text_height;
                        SDL_QueryTexture(tileTexture, NULL, NULL, &text_width, &text_height);
                        SDL_Rect textRect = {
                            tileRect.x + (tileRect.w - text_width) / 2,
                            tileRect.y + (tileRect.h - text_height) / 2,
                            text_width,
                            text_height
                        };
                        SDL_RenderCopy(renderer, tileTexture, NULL, &textRect);
                        SDL_DestroyTexture(tileTexture);
                    }
                }
            }
        }
    }
    
    // 如果游戏结束，显示游戏结束信息
    if (game_state.game_over) {
        printf("正在显示游戏结束画面\n");
        // 半透明叠加
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 192);
        SDL_RenderFillRect(renderer, &gridRect);
        // 显示游戏结束文本
        SDL_Texture* gameOverTexture = render_text("游戏结束!", font_large, titleColor);
        if (gameOverTexture) {
            int text_width, text_height;
            SDL_QueryTexture(gameOverTexture, NULL, NULL, &text_width, &text_height);
            SDL_Rect textRect = {
                GRID_OFFSET_X + (GRID_WIDTH - text_width) / 2,
                GRID_OFFSET_Y + (GRID_HEIGHT - text_height) / 2,
                text_width,
                text_height
            };
            SDL_RenderCopy(renderer, gameOverTexture, NULL, &textRect);
            SDL_DestroyTexture(gameOverTexture);
        }
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    }
    
    // 显示状态信息
    // 如果状态信息超过5秒，恢复默认
    if (status_time > 0 && SDL_GetTicks() - status_time > 5000) {
        strcpy(status_text, "使用方向键或WASD控制");
        status_time = 0;
    }
    SDL_Texture* statusTexture = render_text(status_text, font_small, titleColor);
    if (statusTexture) {
        int text_width, text_height;
        SDL_QueryTexture(statusTexture, NULL, NULL, &text_width, &text_height);
        SDL_Rect statusRect = {20, WINDOW_HEIGHT - 30, text_width, text_height};
        SDL_RenderCopy(renderer, statusTexture, NULL, &statusRect);
        SDL_DestroyTexture(statusTexture);
    }
    
    // 更新屏幕
    SDL_RenderPresent(renderer);
}

// 游戏主循环
void game_loop() {
    SDL_Event event;
    bool quit = false;
    
    while (!quit) {
        // 处理事件
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    quit = true;
                    break;
                case SDL_KEYDOWN:
                    handle_key_input(&event.key);
                    break;
                case SDL_MOUSEMOTION:
                    // 更新按钮悬停状态
                    update_button_hover(&new_game_btn, event.motion.x, event.motion.y);
                    update_button_hover(&undo_btn, event.motion.x, event.motion.y);
                    update_button_hover(&hint_btn, event.motion.x, event.motion.y);
                    update_button_hover(&auto_play_btn, event.motion.x, event.motion.y);
                    update_button_hover(&custom_mode_btn, event.motion.x, event.motion.y);
                    update_button_hover(&apply_custom_btn, event.motion.x, event.motion.y);
                    update_button_hover(&increment_btn, event.motion.x, event.motion.y);
                    update_button_hover(&decrement_btn, event.motion.x, event.motion.y);
                    update_button_hover(&ai_depth_inc_btn, event.motion.x, event.motion.y);
                    update_button_hover(&ai_depth_dec_btn, event.motion.x, event.motion.y);
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        handle_mouse_click(event.button.x, event.button.y);
                    }
                    break;
            }
        }
        
        // 如果AI自动游戏模式开启，立即移动（不在自定义模式下且游戏未结束）
        if (auto_play && !is_game_over(&game_state) && !custom_mode) {
            auto_play_move();
        }
        
        // 渲染游戏
        render_game();
        
        // 控制帧率
        SDL_Delay(16);  // 约60FPS
    }
}

// 启动游戏
int start_game() {
    // 初始化SDL
    if (!init_sdl()) {
        return 1;
    }
    // 初始化游戏表格
    init_tables();
    // 初始化游戏状态
    init_game(&game_state);
    // 主游戏循环
    game_loop();
    // 清理资源
    cleanup();
    return 0;
}

// 主函数
int main(int argc, char* argv[]) {
    // 设置随机数种子
    srand((unsigned int)time(NULL));
    return start_game();
}

// 检查鼠标点击是否在棋盘内，并获取对应的行列
bool get_grid_position(int x, int y, int* row, int* col) {
    // 计算棋盘区域
    int grid_left = GRID_OFFSET_X + GRID_PADDING;
    int grid_top = GRID_OFFSET_Y + GRID_PADDING;
    int grid_right = grid_left + GRID_SIZE * (TILE_SIZE + TILE_MARGIN) - TILE_MARGIN;
    int grid_bottom = grid_top + GRID_SIZE * (TILE_SIZE + TILE_MARGIN) - TILE_MARGIN;
    
    // 检查是否在棋盘范围内
    if (x < grid_left || x >= grid_right || y < grid_top || y >= grid_bottom) {
        return false;
    }
    
    // 计算行列值
    *col = (x - grid_left) / (TILE_SIZE + TILE_MARGIN);
    *row = (y - grid_top) / (TILE_SIZE + TILE_MARGIN);
    
    return true;
}

// 设置棋盘上指定位置的砖块值
void set_tile_value(int row, int col, int value) {
    if (row < 0 || row >= BOARD_SIZE || col < 0 || col >= BOARD_SIZE) {
        return;
    }
    
    // 将值转换为2的指数形式
    int exponent = 0;
    if (value > 0) {
        int tmp = value;
        while (tmp > 1) {
            tmp >>= 1;
            exponent++;
        }
    }
    
    // 读取当前棋盘
    int grid[BOARD_SIZE][BOARD_SIZE];
    board_to_grid(game_state.board, grid);
    
    // 更新指定位置
    grid[row][col] = value;
    
    // 更新棋盘状态
    game_state.board = grid_to_board(grid);
    
    // 显示调试信息
    printf("设置砖块[%d][%d]为%d (2^%d)，新棋盘值：%llu\n", 
                      row, col, value, exponent, game_state.board);} 