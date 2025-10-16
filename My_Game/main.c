#include "device_driver.h"

#define LCDW            (320)
#define LCDH            (240)
#define X_MIN           (0)
#define X_MAX           (LCDW - 1)
#define Y_MIN           (0)
#define Y_MAX           (LCDH - 1)

#define TIMER3_PERIOD   (50) //ms
#define TIMER4_PERIOD   (50) //ms player speed
#define FALL_TIME       (2000.) //ms
#define FALL_COUNT      (FALL_TIME / TIMER3_PERIOD) 
#define RIGHT           (1)
#define LEFT            (-1)

#define GRAVITY         (1)
#define JUMP_SPEED      (-8)
#define MAX_FALL_SPEED  (10)

#define BULLET_STEP     (15)  // 총알의 이동 크기
#define BULLET_SIZE_X   (5)   // 총알 크기 (가로)
#define BULLET_SIZE_Y   (5)   // 총알 크기 (세로)
#define MAX_BULLET 		(4)	 // 총알 최대 개수

#define PLAYER_STEP     (5)
#define PLAYER_SIZE_X   (10)
#define PLAYER_SIZE_Y   (10)

#define BOSS_COLOR       (7)
#define BOSS_PAT_COLOR   (9) 
#define BOSS_BL_COLOR    (6)
#define MAX_BOSS_BULLETS (40)
#define MAX_BOSS_PATTERNS (10)
#define BOSS_HP         (35)

#define ITEM_SIZE_X     (6)
#define ITEM_SIZE_Y     (6)

#define MAX_PLATFORM    (20)
#define MAX_WALL        (20)

#define MAX_ITEM        (8)
#define BACK_COLOR      (5)
#define BULLET_COLOR    (2)  // 총알 색상
#define PLAYER_COLOR    (1)
#define WALL1_COLOR     (8)
#define WALL_COLOR      (0)
#define ITEM_COLOR      (3)

#define GAME_OVER       (1)

static int game_over = 0;
static int clear = 0;
static int Wall1_Flag = 1;
static int Wall2_Flag = 0;
static int currentStage = 0;
static int platformCount;
static int wallCount;
static int itemCount;
extern volatile uint32_t ms_tick;
static int zig_flag = 0;
static int invert_flag = 1;
static int bounding_flag = 0;
static int x_dir = 1, y_dir = 1;
static int x_min, x_max;
static int score = 0;

typedef struct {
    int x, y;
    int w, h;
    int ci;
    int dir;
    int vx, vy;
    int is_Jumping;   // jump 유무
    int jumpCount;    // jump 횟수 
    int item_enable;   
} QUERY_DRAW;

typedef struct {
    int x, y, w, h, ci;
    int vx, vy;       // 벽, 발판, 아이템 간소화 구조체체
    int item_enable;
} ObjDef;

typedef enum {
    MOVE_NONE,
    MOVE_CHANCE,
    MOVE_VERTICAL,
    MOVE_HORIZONTAL,
    MOVE_ZIGZAG,
    MOVE_BOUNDING
} BossMovePattern;

typedef enum {
    BULLET_NONE,
    BULLET_SINGLE,
    BULLET_SPREAD,
    BULLET_AIM,       
    BULLET_PATTERN_COUNT
} BossBulletPattern;

typedef struct {
    BossMovePattern  movePat;
    BossBulletPattern bulletPat;
    uint32_t         duration_ms;  // 이 패턴을 이만큼(ms) 실행
} BossPatternEntry;


typedef struct {
    int x,y,w,h,ci;

    int moveSpeed;
    int moveRangeMin, moveRangeMax;
    int dir;

    // 패턴 리스트
    BossPatternEntry patterns[MAX_BOSS_PATTERNS];
    int              numPatterns;
    int              currentPattern;  
    uint32_t         patternTimer;    

    // 체력
    int hp;

    // 총알 설정
    int               numBullets;
    int               bulletSpeed;
    int               fireInterval;
    uint32_t          lastFire;
    uint32_t          lastPatternTime;
    ObjDef            bullets[MAX_BOSS_BULLETS];
} BossDef;

typedef struct {
    int  numPlatforms;
    ObjDef platforms[MAX_PLATFORM];

    int  numWalls;
    ObjDef walls[MAX_WALL];

    int  numitems;
    ObjDef items[MAX_ITEM];

    BossDef boss;
} StageDef;


static QUERY_DRAW items[MAX_ITEM];
static QUERY_DRAW walls[MAX_WALL];
static QUERY_DRAW platforms[MAX_PLATFORM];
static QUERY_DRAW bullet[MAX_BULLET];  // 총알 변수로 변경
static QUERY_DRAW player;
static BossDef bossRuntime;

static unsigned short color[] = {RED, YELLOW, GREEN, BLUE, WHITE, BLACK, VIOLET, GRAY, ORANGE, MAROON};

static void Draw_Object(QUERY_DRAW* obj) {
    Lcd_Draw_Box(obj->x, obj->y, obj->w, obj->h, color[obj->ci]);
}

static void Draw_Boss(const BossDef *b) {
    // (BossDef 필드 순서: x,y,w,h,ci,...)
    Lcd_Draw_Box(b->x, b->y, b->w, b->h, color[b->ci]);
}

static void MovePattern_None(BossDef *b) {
    b->ci = BOSS_COLOR;
    b->fireInterval = 700;
    b->moveSpeed = 3;
    zig_flag = 0;
    bounding_flag = 0;
}

static void MovePattern_Chance_None(BossDef *b){
    b->ci = BOSS_COLOR;
    b->fireInterval = 700;
    b->moveSpeed = 3;
    zig_flag = 0;
    bounding_flag = 0;
    b->y += b->moveSpeed;
    if(b->y > Y_MAX - b->w - 7){
        b->y = Y_MAX - b->w - 7;
    }
}

static void MovePattern_Vertical(BossDef *b) {
    if(invert_flag == -1) b->x = 10;
    else b->x = X_MAX - b->w - 10;
    b->y += b->dir * b->moveSpeed;
    b->ci = BOSS_COLOR;
    // 패턴 범위
    if (b->y < Y_MIN + 7){
        b->y = Y_MIN + 7;      
        b->dir = +1;
    }
    if (b->y + b->h > Y_MAX - 7)  {
        b->y = Y_MAX - b->h - 7;
        b->dir = -1;
    }
}

// 수평 왕복 + 화면 경계 클램프
static void MovePattern_Horizontal(BossDef *b) {
    b->y = 10;
    b->moveSpeed = 7;
    b->x += b->dir * b->moveSpeed;
    b->fireInterval = 200;
    b->ci = BOSS_COLOR;
    // 패턴 범위
    // LCD 전체 화면 클램프
    if (b->x < X_MIN + 5){
        b->x = X_MIN + 5;
        b->dir = +1;
    }
    if (b->x + b->w > X_MAX - 5){
        b->x = X_MAX - b->w - 5;
        b->dir = -1;
    }
}

// 지그재그 + 화면 경계 클램프
static void MovePattern_Zigzag(BossDef *b) {
    b->fireInterval = 500;
    if(zig_flag == 0){
        b->x = X_MAX / 2;
        x_min = b->x - b->moveRangeMin;
        x_max = b->x + b->moveRangeMax;
        b->moveSpeed = 10;
        b->y = 10;
        zig_flag = 1;
        invert_flag *= -1;
    }
    
    b->x += b->dir * b->moveSpeed;
    b->ci = BOSS_COLOR;
    // 패턴 범위 체크
    if (b->x < x_min || b->x + b->w > x_max) {
        b->dir = -b->dir;
    }

    // LCD 전체 화면 클램프
    if (b->x < X_MIN+ 5)      b->x = X_MIN + 5;
    if (b->x + b->w > X_MAX - 5)  b->x = X_MAX - b->w - 5;
    if (b->y < Y_MIN + 5)         b->y = Y_MIN + 5;
    if (b->y + b->h > Y_MAX - 5)  b->y = Y_MAX - b->h - 5;
}

static void MovePattern_Bounding(BossDef *b){
    if(bounding_flag == 0){
        x_dir = 1, y_dir = 1;
        bounding_flag = 1;
    }
    b->ci = BOSS_PAT_COLOR;
    b->moveSpeed = 10;
    b->x += x_dir * b->moveSpeed;
    b->y += y_dir * b->moveSpeed;

    if (b->x < X_MIN+ 5){     
        b->x = X_MIN + 5;
        x_dir *= -1;
    }
    if (b->x + b->w > X_MAX - 5) {
        b->x = X_MAX - b->w - 5;
        x_dir *= -1;
    }
    if (b->y < Y_MIN + 5) {
        b->y = Y_MIN + 5;
        y_dir *= -1;
    }
    if (b->y + b->h > Y_MAX - 5)  {
        b->y = Y_MAX - b->h - 5;
        y_dir *= -1;
    }
}

static void (*MovePatterns[])(BossDef*) = {
    MovePattern_None,
    MovePattern_Chance_None,
    MovePattern_Vertical,
    MovePattern_Horizontal,
    MovePattern_Zigzag,
    MovePattern_Bounding
};

// ──────────────── 발사 패턴 함수 ────────────────
static void BulletPattern_None(void){

}

static void BulletPattern_Single(BossDef *b) {
    int i;
    
    for (i = 0; i < b->numBullets; i++) {
        ObjDef *bl = &b->bullets[i];
        int dx = (player.x + player.w/2) - (b->x + b->w/2);
        dx = (dx > 0) ? 1:-1;
        if (!bl->item_enable) {
            bl->w = 5; bl->h = 10; bl->ci = BOSS_BL_COLOR;
            bl->x = b->x + b->w/2 - bl->w/2;
            bl->y = b->y + b->h;
            bl->vx = b->bulletSpeed * dx; bl->vy = 0;
            bl->item_enable = 1;
            break;
        }
    }
}

static void BulletPattern_Spread(BossDef *b) {
    const float baseAngle = M_PI_2;      // 아래 방향: +90도
    const float delta     = M_PI / 12.0; // 15도
    int i;
    // 3개의 발사 각도: 75°, 90°, 105°
    float angles[5] = {
        baseAngle - 2* delta,
        baseAngle - delta,
        baseAngle,
        baseAngle + delta,
        baseAngle + 2* delta
    };

    int shots = 0;
    for (i = 0; i < b->numBullets && shots < 5; i++) {
        ObjDef *bl = &b->bullets[i];
        if (bl->item_enable) continue;

        // 탄 기본 설정
        bl->w = 5;  bl->h = 10;
        bl->ci = BOSS_BL_COLOR;
        bl->x  = b->x + b->w/2 - bl->w/2;
        bl->y  = b->y + b->h;

        // 속도 분해: vx = speed * cos(θ), vy = speed * sin(θ)
        float ang = angles[shots];
        bl->vx = (int)(b->bulletSpeed * cosf(ang));
        bl->vy = (int)(b->bulletSpeed * sinf(ang));

        bl->item_enable = 1;
        shots++;
    }
}

static void BulletPattern_AimPlayer(BossDef *b) {
    int i;
    
    for (i = 0; i < b->numBullets; i++) {
        ObjDef *bl = &b->bullets[i];
        if (!bl->item_enable) {
            // 기본 치수
            bl->w = 5; bl->h = 10; bl->ci = BOSS_BL_COLOR;
            bl->x = b->x + b->w/2 - bl->w/2;
            bl->y = b->y + b->h;

            // 플레이어 방향 계산
            int dx = (player.x + player.w/2) - (bl->x + bl->w/2);
            int dy = (player.y + player.h/2) - (bl->y + bl->h/2);
            float d = sqrtf((float)dx*dx + (float)dy*dy);
            if (d < 1.0f) d = 1.0f;

            // 속도 벡터
            bl->vx = (int)(b->bulletSpeed * dx / d);
            bl->vy = (int)(b->bulletSpeed * dy / d);

            bl->item_enable = 1;
            break;
        }
    }
}

// 3) 기존 패턴들 유지, Aim 추가
static void (*BulletPatterns[BULLET_PATTERN_COUNT])(BossDef*) = {
    BulletPattern_None,
    BulletPattern_Single,
    BulletPattern_Spread,
    BulletPattern_AimPlayer
};

static const StageDef stages[] = {
    // Stage 0
    {
       .numPlatforms = 9,
       .platforms = {
           { 0,233,40,7,WALL1_COLOR, 0 ,0 }, { 80,200,20,7,WALL1_COLOR, 0 ,0 }
           ,{140, 170, 20, 7, WALL1_COLOR, 0, 0 }, { 90, 140, 20, 7, WALL1_COLOR, 0, 0 }
           ,{160, 110, 20, 7, WALL1_COLOR, 0, 0 }, {120,  80, 20, 7, WALL1_COLOR, 0, 0 }
           ,{200,  50, 20, 7, WALL1_COLOR, 0, 0 }, {260,  20, 20, 7, WALL1_COLOR, 0, 0 }
           ,{295,  233, 25, 7, WALL1_COLOR, 0, 0 }
       },
       .numWalls = 3,
       .walls = {
           {280, 40,  5,200,WALL_COLOR,   0,0},  
           {120, Y_MIN , 50,  7,BACK_COLOR,    0, 5}, 
           {  0, 15, 50,  7,BACK_COLOR,   20, 0}, 
       }
    },

    // Stage 1
    {
        .numPlatforms = 9,
        .platforms = {
          { 0,233,25,7,WALL1_COLOR, 0 ,0 },
          { .x= 60, .y=200, .w=20, .h=20, .ci=WALL1_COLOR, .vx=0, .vy=0 },
          { .x= 120, .y=170, .w=20, .h=20, .ci=WALL1_COLOR, .vx=0, .vy=0 },
          { .x= 50, .y=125, .w=20, .h=20, .ci=WALL1_COLOR, .vx=0, .vy=0 },
          { .x= 130, .y=95, .w=20, .h=20, .ci=WALL1_COLOR, .vx=0, .vy=0 },
          { .x= 70, .y= 45, .w=20, .h=20, .ci=WALL1_COLOR, .vx=0, .vy=0 },
          { 140,35,40,7,WALL1_COLOR, 0 ,0 },
          {285,  233, X_MAX - 285, 7, WALL1_COLOR, 0, 0 },
          { 0,0,X_MAX,10,WALL1_COLOR, 0 ,0 }
          
        },
        .numWalls = 4,
        .walls = {
          { .x=190, .y=40, .w=10, .h=Y_MAX-40, .ci=WALL_COLOR, .vx=0, .vy=0 },

          { .x=230, .y=10, .w=10, .h=100, .ci=WALL_COLOR, .vx=0, .vy=0 },

          { .x=270, .y=40, .w=10, .h=Y_MAX-40, .ci=WALL_COLOR, .vx=0, .vy=0 },

        },
        .numitems = 5,
        .items = {
            {.x=213, .y=105, .w = ITEM_SIZE_X, .h = ITEM_SIZE_Y, .ci=ITEM_COLOR, .item_enable = 1 },
            {.x=233, .y=130, .w = ITEM_SIZE_X, .h = ITEM_SIZE_Y, .ci=ITEM_COLOR, .item_enable = 1 },
            {.x=253, .y=105, .w = ITEM_SIZE_X, .h = ITEM_SIZE_Y, .ci=ITEM_COLOR, .item_enable = 1 },
            {.x=253, .y=75, .w = ITEM_SIZE_X, .h = ITEM_SIZE_Y, .ci=ITEM_COLOR, .item_enable = 1 },
            {.x=253, .y=45, .w = ITEM_SIZE_X, .h = ITEM_SIZE_Y, .ci=ITEM_COLOR, .item_enable = 1 }
        }
      },
    // Stage 2
    {
        .numPlatforms = 14,
        .platforms = {
          { 0,233,40,7,WALL1_COLOR, 0 ,0 },{ 30,200,25,7,WALL1_COLOR, 0 ,0 },
          { 0,170,25,7,WALL1_COLOR, 0 ,0 },{ 30,140,25,7,WALL1_COLOR, 0 ,0 },
          /*기둥1*/{ 75,100,15,Y_MAX-100,WALL1_COLOR, 0 ,0 },{ 55,20,13,50,WALL1_COLOR, 0 ,0 },
          { 0,10,X_MAX,10,WALL1_COLOR, 0 ,0 },{ 140,20,17,180,WALL1_COLOR, 0 ,0 }/*기둥2*/,
          { 130,232,70,7,WALL1_COLOR, 0 ,0 },{ 200,90,15,Y_MAX-90,WALL1_COLOR, 0 ,0 }/*기둥3*/,
          { 270,210,20,7,WALL1_COLOR, 0 ,0 }, { 290,180,20,7,WALL1_COLOR, 0 ,0 },
          { 310,160,X_MAX-310,Y_MAX-160,WALL1_COLOR, 0 ,0}, { 240,20,15,160,WALL1_COLOR, 0 ,0}/*기둥4*/
        },
        .numWalls = 9,
        .walls = {
          { .x=55, .y=70, .w=13, .h=13, .ci=WALL_COLOR, .vx=0, .vy=0 },{ .x=115, .y=120, .w=25, .h=8, .ci=WALL_COLOR, .vx=0, .vy=0 },
          { .x=90, .y=200, .w=25, .h=8, .ci=WALL_COLOR, .vx=0, .vy=0 },
          { .x=157, .y=187, .w=15, .h=13, .ci=WALL_COLOR, .vx=0, .vy=0 },{ .x=185, .y=167, .w=15, .h=13, .ci=WALL_COLOR, .vx=0, .vy=0 },
          { .x=157, .y=147, .w=15, .h=13, .ci=WALL_COLOR, .vx=0, .vy=0 },{ .x=185, .y=127, .w=15, .h=13, .ci=WALL_COLOR, .vx=0, .vy=0 },
          { .x=157, .y=107, .w=15, .h=13, .ci=WALL_COLOR, .vx=0, .vy=0 },{ .x=200, .y=75, .w=15, .h=15, .ci=WALL_COLOR, .vx=0, .vy=0 }
        },
        .numitems = 8,
        .items = {
            {.x=167, .y=180, .w = ITEM_SIZE_X, .h = ITEM_SIZE_Y, .ci=ITEM_COLOR, .item_enable = 1 },
            {.x=167, .y=140, .w = ITEM_SIZE_X, .h = ITEM_SIZE_Y, .ci=ITEM_COLOR, .item_enable = 1 },
            {.x=167, .y=100, .w = ITEM_SIZE_X, .h = ITEM_SIZE_Y, .ci=ITEM_COLOR, .item_enable = 1 },
            {.x=185, .y=160, .w = ITEM_SIZE_X, .h = ITEM_SIZE_Y, .ci=ITEM_COLOR, .item_enable = 1 },
            {.x=185, .y=120, .w = ITEM_SIZE_X, .h = ITEM_SIZE_Y, .ci=ITEM_COLOR, .item_enable = 1 },
            {.x=185, .y=75, .w = ITEM_SIZE_X, .h = ITEM_SIZE_Y, .ci=ITEM_COLOR, .item_enable = 1 },
            {.x=225, .y=195, .w = ITEM_SIZE_X, .h = ITEM_SIZE_Y, .ci=ITEM_COLOR, .item_enable = 1 },
            {.x=250, .y=210, .w = ITEM_SIZE_X, .h = ITEM_SIZE_Y, .ci=ITEM_COLOR, .item_enable = 1 },


        },
    },
    // Boss Stage
    {
        .numPlatforms = 11,
        .platforms = {
            { 0,233,X_MAX,7,WALL1_COLOR, 0 ,0 },{ 10,200,50,7,WALL1_COLOR, 0 ,0 },
            { 10,160,50,7,WALL1_COLOR, 0 ,0 },{ 10,120,50,7,WALL1_COLOR, 0 ,0 }
            ,{ 10,80,50,7,WALL1_COLOR, 0 ,0 },{ 0,0,X_MAX,10,4, 0 ,0 }/*체력바(index = 5)*/
            ,{ X_MAX-60,200,50,7,WALL1_COLOR, 0 ,0 },{  X_MAX-60,160,50,7,WALL1_COLOR, 0 ,0 },
            {  X_MAX-60,120,50,7,WALL1_COLOR, 0 ,0 },{  X_MAX-60,80,50,7,WALL1_COLOR, 0 ,0 },
            { X_MAX,0,0,10,5, 0 ,0 }/*체력바 줄어드는 검정*/

        },
        .numWalls = 0,
        .numitems = 0,
        .boss = {
            .x = X_MAX-50, .y = Y_MIN+40, .w = 40, .h = 40, .ci = BOSS_COLOR,
            .moveSpeed    = 3,
            .moveRangeMin = 130, .moveRangeMax = 130,
            .dir          = +1,
            .patterns = {
              { MOVE_VERTICAL,   BULLET_SINGLE, 8000},
              { MOVE_NONE,        BULLET_NONE,  2500},
              { MOVE_ZIGZAG,     BULLET_SPREAD,    5000 },
              { MOVE_NONE,        BULLET_NONE,  2000},
              { MOVE_HORIZONTAL, BULLET_AIM, 7000 },
              { MOVE_NONE,        BULLET_NONE,  2000},
              { MOVE_BOUNDING,    BULLET_NONE,  5000},
              { MOVE_CHANCE,        BULLET_NONE,  4000}
            },
            .numPatterns    = 8,
            .currentPattern = 0,
            .patternTimer   = 0,
            .hp             = BOSS_HP,
            .numBullets     = MAX_BOSS_BULLETS,
            .bulletSpeed    = 5,
            .fireInterval   = 700,
            .lastFire       = 0,
            .bullets        = { {0} },
          }
    },

    // Clear
    {
        .numPlatforms = 4,
        .platforms = {
            // 윗쪽 테두리
            { 
                .x  = X_MIN, 
                .y  = Y_MIN, 
                .w  = X_MAX, 
                .h  = 10, 
                .ci = 4
            },
            // 아랫쪽 테두리
            { 
                .x  = X_MIN, 
                .y  = Y_MAX - 9,  // Y_MAX = LCDH-1 이므로 두께 10을 위해 -9
                .w  = X_MAX, 
                .h  = 10, 
                .ci = 4
            },
            // 왼쪽 테두리
            { 
                .x  = X_MIN, 
                .y  = Y_MIN, 
                .w  = 10, 
                .h  = Y_MAX, 
                .ci = 4 
            },
            // 오른쪽 테두리
            { 
                .x  = X_MAX - 9, // X_MAX = LCDW-1 이므로 두께 10을 위해 -9
                .y  = Y_MIN, 
                .w  = 10, 
                .h  = Y_MAX, 
                .ci = 4
            }
        }
    },
    //Score Boss
    {
        .numPlatforms = 11,
        .platforms = {
            { 0,233,X_MAX,7,WALL1_COLOR, 0 ,0 },{ 10,200,50,7,WALL1_COLOR, 0 ,0 },
            { 10,160,50,7,WALL1_COLOR, 0 ,0 },{ 10,120,50,7,WALL1_COLOR, 0 ,0 }
            ,{ 10,80,50,7,WALL1_COLOR, 0 ,0 },{ 0,0,X_MAX,10,4, 0 ,0 }/*체력바(index = 5)*/
            ,{ X_MAX-60,200,50,7,WALL1_COLOR, 0 ,0 },{  X_MAX-60,160,50,7,WALL1_COLOR, 0 ,0 },
            {  X_MAX-60,120,50,7,WALL1_COLOR, 0 ,0 },{  X_MAX-60,80,50,7,WALL1_COLOR, 0 ,0 },
            { X_MAX,0,0,10,5, 0 ,0 }/*체력바 줄어드는 검정*/

        },
        .numWalls = 0,
        .numitems = 0,
        .boss = {
            .x = X_MAX-50, .y = Y_MIN+40, .w = 40, .h = 40, .ci = BOSS_COLOR,
            .moveSpeed    = 3,
            .moveRangeMin = 130, .moveRangeMax = 130,
            .dir          = +1,
            .patterns = {
              { MOVE_VERTICAL,   BULLET_SINGLE, 8000},
              { MOVE_NONE,        BULLET_NONE,  2500},
              { MOVE_ZIGZAG,     BULLET_SPREAD,    5000 },
              { MOVE_NONE,        BULLET_NONE,  2000},
              { MOVE_HORIZONTAL, BULLET_AIM, 7000 },
              { MOVE_NONE,        BULLET_NONE,  2000},
              { MOVE_BOUNDING,    BULLET_NONE,  5000},
              { MOVE_CHANCE,        BULLET_NONE,  4000}
            },
            .numPatterns    = 8,
            .currentPattern = 0,
            .patternTimer   = 0,
            .hp             = BOSS_HP,
            .numBullets     = MAX_BOSS_BULLETS,
            .bulletSpeed    = 5,
            .fireInterval   = 700,
            .lastFire       = 0,
            .bullets        = { {0} },
          }
    }
};

static int Collides(QUERY_DRAW *obj, int x, int y, int w, int h) {
    // obj->x,y는 좌상단, w,h는 폭·높이
    return (x < obj->x + obj->w) &&
           (x + w > obj->x) &&
           (y < obj->y + obj->h) &&
           (y + h > obj->y);
}



//------------------------------------------------------------------------------
// 플레이어 ↔ 발판 충돌 처리
//------------------------------------------------------------------------------
static void CheckPlayerPlatform(void) {
    int i, on_platform = 0;

    if (player.vx != 0) {
        int new_x = player.x + player.vx;
        for (i = 0; i < platformCount; i++) {
            QUERY_DRAW *pf = &platforms[i];
            if (pf->ci != WALL1_COLOR) continue;

            if (Collides(pf, new_x, player.y, player.w, player.h)) {
                // 왼쪽으로 이동하던 중 충돌
                if (player.vx < 0) {
                    player.x = pf->x + pf->w;
                }
                // 오른쪽으로 이동하던 중 충돌
                else {
                    player.x = pf->x - player.w;
                }
                player.vx = 0;
                break;
            }
        }
    }

    if (player.vy != 0) {
        int new_y = player.y + player.vy;
        for (i = 0; i < platformCount; i++) {
            QUERY_DRAW *pf = &platforms[i];
            if (pf->ci != WALL1_COLOR) continue;

            if (Collides(pf, player.x, new_y, player.w, player.h)) {
                if (player.vy > 0) {
                    player.y = pf->y - player.h;
                    player.is_Jumping = 0;
                    player.vy = 0;
                    player.jumpCount = 0;  
                }
                else {
                    player.y = pf->y + pf->h;
                    player.vy = 0;
                }
                break;
            }
        }
    }

    for (i = 0; i < platformCount; i++) {
        QUERY_DRAW *pf = &platforms[i];
        if (pf->ci != WALL1_COLOR) continue;

        if ((player.y + player.h == pf->y) &&
            (player.x + player.w  > pf->x) &&
            (player.x < pf->x + pf->w)) {
            on_platform = 1;
            break;
        }
    }

    if (!on_platform) {
        player.is_Jumping = 1;
    }

    // 5) 바닥으로 낙하 시 게임 오버
    if (!on_platform && player.y + player.h > Y_MAX) {
        player.y = Y_MAX - player.h;
        game_over = 1;
    }
}


//------------------------------------------------------------------------------
// 플레이어 ↔ 벽 충돌 처리
//------------------------------------------------------------------------------
static void CheckPlayerWall(void) {
    // 가상 천장
    int i;
    QUERY_DRAW top_wall = { 0, -100, LCDW, 100, 0 };
    if (Collides(&top_wall, player.x, player.y, player.w, player.h))
    {
        player.y = Y_MIN;
    }

    // 가상 오른쪽 끝 벽(스테이지 클리어)
    QUERY_DRAW right_wall = { X_MAX - 2, 0, 100, Y_MAX,  BACK_COLOR };
    if (Collides(&right_wall,player.x, player.y,player.w, player.h))
    {
        player.x = X_MAX - player.w;
        
        if(currentStage < 3) clear = 1;
    }

    // 실제 벽
    for ( i = 0; i < wallCount; i++) {
        if (Collides(&walls[i], player.x, player.y, player.w, player.h) && walls[i].ci == WALL_COLOR)
        {
            game_over = GAME_OVER;
            return;
        }
    }
}


//------------------------------------------------------------------------------
// 총알 ↔ 발판/벽 충돌 처리
//------------------------------------------------------------------------------
static void CheckBulletCollisions(int b) {
    int i;
    QUERY_DRAW *bl = &bullet[b];
    // 발판 충돌
    for (i = 0; i < platformCount; i++) {
        QUERY_DRAW * pl = &platforms[i];
        if (Collides(pl,bl->x, bl->y, bl->w, bl->h) && pl -> ci == WALL1_COLOR)
        {
            bl -> ci = BACK_COLOR;
            Draw_Object(bl);
            bl->dir = 0;
            Draw_Object(&platforms[i]);
        }
    }

    // 벽 충돌
    for (i = 0; i < wallCount; i++) {
        if (Collides(&walls[i], bl->x, bl->y,bl->w, bl->h))
        {
            bl -> ci = BACK_COLOR;
            Draw_Object(bl);
            bl->dir = 0;
            Draw_Object(&walls[i]);
        }
    }

    // 화면의 끝 충돌
    if (bl->x < X_MIN || bl->x + bl->w > X_MAX) {
        bl -> ci = BACK_COLOR;
        Draw_Object(bl);
        bl->dir = 0;
    }
}

//------------------------------------------------------------------------------
// 통합 충돌 검사
//------------------------------------------------------------------------------
static void Collision_Check(void) {
    CheckPlayerPlatform();
    CheckPlayerWall();
}

static int MoveWall_y(int i) {
    int j;
    QUERY_DRAW *w = &walls[i];
    w->ci = BACK_COLOR;
    Draw_Object(w);

    w->y += w->vy;

    if (w->y + w->h > Y_MAX) {
        w->y = Y_MIN;
        return 0;
    }
    
    w->ci = WALL_COLOR;
    Draw_Object(w);

    for (j=0; j<MAX_PLATFORM; j++) {
        Draw_Object(&platforms[j]);
    }

    for (j=0; j< MAX_WALL; j++) {
        if (Collides(&walls[j], walls[i].x, walls[i].y, walls[i].w, walls[i].h))
            Draw_Object(&walls[j]);
    }

    return 1;
}

static int MoveWall_x(int i) {
    int j;
    QUERY_DRAW *w = &walls[i];
    // 1) 이전 위치 지우기
    w->ci = BACK_COLOR;
    Draw_Object(w);

    // 2) 위치 갱신
    w->x += w->vx;

    if (w->x + w->w > X_MAX) {
        w->x = 0;
        Wall2_Flag = 2;
        return 0;
    }

    w->ci = WALL_COLOR;
    Draw_Object(w);

    for (j=0; j<MAX_PLATFORM; j++) {
        if (Collides(&platforms[j], walls[i].x, walls[i].y, walls[i].w, walls[i].h))
            Draw_Object(&platforms[j]);
    }

    for (j=0; j< MAX_WALL; j++) {
        if (Collides(&walls[j], walls[i].x, walls[i].y, walls[i].w, walls[i].h))
            Draw_Object(&walls[i]);
    }

    return 1;
}

static void Toggle_Platforms(void) {
    static int toggle = 0;  // 0: 1,3,5 표시 / 1: 2,4,6 표시

    int i;
    for (i = 1; i < 6; i++) {
        // 홀수만 처리하거나 짝수만 처리
        if ((i % 2 == 1 && toggle == 0) || (i % 2 == 0 && toggle == 1)) {
            platforms[i].ci = WALL1_COLOR;  // 다시 그릴 색상
        } else {
            platforms[i].ci = BACK_COLOR;   // 배경색으로 지우기
        }
        Draw_Object(&platforms[i]);
    }

    toggle = !toggle; // 상태 전환
}

static void Toggle_Walls(void) {
    static int toggle = 0;  // 0: 1,3,5 표시 / 1: 2,4,6 표시

    int i;
    for (i = 3; i < 8; i++) {
        // 홀수만 처리하거나 짝수만 처리
        if ((i % 2 == 1 && toggle == 0) || (i % 2 == 0 && toggle == 1)) {
            walls[i].ci = WALL_COLOR;  // 다시 그릴 색상
        } else {
            walls[i].ci = BACK_COLOR;   // 배경색으로 지우기
        }
        Draw_Object(&walls[i]);
    }

    toggle = !toggle; // 상태 전환
}


static void jump(void) {
    if (player.jumpCount < 2) {
        player.vy = JUMP_SPEED;
        player.is_Jumping = 1;    // 공중 상태
        player.jumpCount++;       // 점프 카운트 증가
    }
}

static void Apply_Gravity(void) {
    int i, j;
    if (player.is_Jumping) {
        player.vy += GRAVITY;
        if (player.vy > MAX_FALL_SPEED) player.vy = MAX_FALL_SPEED;

        int step = (player.vy > 0) ? 1 : -1;
        int count = (player.vy > 0) ? player.vy : -player.vy;
        for(i = 0; i < count; i++){
            for(j = 0; j < itemCount; j++){
                QUERY_DRAW *it = &items[j];
    
                if (it->item_enable != 1) continue;
    
                if (Collides(it, player.x, player.y, player.w, player.h)) {
                    it->item_enable = 0;
                    it->ci = BACK_COLOR;
                    Draw_Object(it);
    
                    if (player.jumpCount > 0) {
                        player.jumpCount--;
                    }
                }
            }
            player.y += step;
        }

    }
    Collision_Check();
}


static void shoot(void){
	int i;
	for (i = 0; i < MAX_BULLET; i++) {
		if (bullet[i].dir == 0) {  // 사용하지 않는 총알 찾기 (dir == 0이면 대기상태)
			if (player.dir == RIGHT) bullet[i].x = player.x + player.w;
			else bullet[i].x = player.x - player.w;
			bullet[i].y = player.y;
			bullet[i].w = BULLET_SIZE_X;
			bullet[i].h = BULLET_SIZE_Y;
			bullet[i].ci = BULLET_COLOR;
			bullet[i].dir = player.dir;  // 방향 설정
			break;  // 하나만 발사
		}
	}
}
	

static void k0(void) {
}

static void k1(void) {
    clear = 1;
}

static void k2(void) {
    if (player.x > X_MIN) {
        player.vx = -PLAYER_STEP;
    }
    else player.vx = 0;
	player.dir = LEFT;
}

static void k3(void) {
    if (player.x + player.w < X_MAX) {
        player.vx = PLAYER_STEP;
    }
	player.dir = RIGHT;
}

static void k4(void) {
	shoot();
}

static void k5(void) {
	jump();
}

static void Player_Move(int k) {
    
    int i, j;
    static void (*key_func[])(void) = {k0, k1, k2, k3, k4, k5};

    if (k < 4) {
        key_func[k]();  // 이동
    }
    int step = (player.vx > 0) ? 1 : -1;
    int count = (player.vx > 0) ? player.vx : -player.vx;
    for(i = 0; i < count; i++){
        for(j = 0; j < MAX_ITEM; j++){
            QUERY_DRAW *it = &items[j];

            if (it->item_enable != 1) continue;

            if (Collides(it, player.x, player.y, player.w, player.h)) {
                it->item_enable = 0;
                it->ci = BACK_COLOR;
                Draw_Object(it);

                if (player.jumpCount > 0) {
                    player.jumpCount--;
                }
            }
        }
        player.x += step;

    }
    Collision_Check();

}

static void Bullet_Move(void){
    int i, j;
	for (i = 0; i < MAX_BULLET; i++) {
        if (bullet[i].dir == 0) continue;

        bullet[i].ci = BACK_COLOR;
        Draw_Object(&bullet[i]);

        if (bullet[i].dir == RIGHT) {
            for (j = 1; j <= BULLET_STEP; j++) {
                bullet[i].x++;
                CheckBulletCollisions(i);
                if (bullet[i].dir == 0) break;  
            }
        } else {
            for (j = 1; j <= BULLET_STEP; j++) {
                bullet[i].x--;
                CheckBulletCollisions(i);
                if (bullet[i].dir == 0) break;
            }
        }

        if (bullet[i].dir == 0) {
            continue;
        }

        bullet[i].ci = BULLET_COLOR;
        Draw_Object(&bullet[i]);
    }
}

static void Boss_Move(void){
    uint32_t now = ms_tick;
    int i, pi;
    bossRuntime.ci = BACK_COLOR;
    Draw_Boss(&bossRuntime);
    for (i = 0; i < bossRuntime.numBullets; i++) {
        ObjDef *bl = &bossRuntime.bullets[i];
        bl->ci = BACK_COLOR;
        if (!bl->item_enable) continue;
        // 임시 QUERY_DRAW에 BACK_COLOR 설정
        QUERY_DRAW tmp_clear = {
            .x  = bl->x,
            .y  = bl->y,
            .w  = bl->w,
            .h  = bl->h,
            .ci = bl->ci
        };
        Draw_Object(&tmp_clear);
    }

    for (pi = 0; pi < platformCount; pi++) {
        Draw_Object(&platforms[pi]);
    }

    // —————— 패턴 시간 갱신 & 전환 ——————
    BossPatternEntry *pe = &bossRuntime.patterns[bossRuntime.currentPattern];
    if (now - bossRuntime.lastPatternTime >= pe->duration_ms) {
        bossRuntime.lastPatternTime = now;
        bossRuntime.currentPattern = (bossRuntime.currentPattern + 1) % bossRuntime.numPatterns;
        pe = &bossRuntime.patterns[bossRuntime.currentPattern];
    }

    if (pe->movePat < sizeof(MovePatterns)/sizeof(*MovePatterns)) {
        MovePatterns[ pe->movePat ](&bossRuntime);
    }

    if ((now - bossRuntime.lastFire) >= bossRuntime.fireInterval) {
        bossRuntime.lastFire = now;
        // 범위 검사
        if ((unsigned)pe->bulletPat < BULLET_PATTERN_COUNT) {
            BulletPatterns[ pe->bulletPat ](&bossRuntime);
        }
    }


    // —————— 이미 발사된 총알들 위치 갱신 ——————
    for (i = 0; i < bossRuntime.numBullets; i++) {
        ObjDef *bl = &bossRuntime.bullets[i];
        if (!bl->item_enable) continue;
        bl->x += bl->vx;
        bl->y += bl->vy;
    }


    for (i = 0; i < bossRuntime.numBullets; i++) {
        ObjDef *bl = &bossRuntime.bullets[i];
        if (!bl->item_enable) continue;
        // 임시 QUERY_DRAW에 BACK_COLOR 설정
        QUERY_DRAW tmp_col = {
            .x  = bl->x,
            .y  = bl->y,
            .w  = bl->w,
            .h  = bl->h,
            .ci = bl->ci
        };
        if (tmp_col.y < Y_MIN + 5 ||tmp_col.x < X_MIN + 5 ||tmp_col.y + tmp_col.h > Y_MAX - 5 ||tmp_col.x + tmp_col.w > X_MAX - 5)
        {
            // 원위치 복귀
            tmp_col.x -= bl->vx;
            tmp_col.y -= bl->vy;
            // 지우기
            tmp_col.ci = BACK_COLOR;
            Draw_Object(&tmp_col);
            bl->item_enable = 0;
            continue;
        }
    
        // d) 플레이어 충돌 검사 → game over
        if (Collides(&player, tmp_col.x, tmp_col.y,tmp_col.w, tmp_col.h))
        {
            game_over = GAME_OVER;
            bl->item_enable = 0;
            continue;
        }

    }

    // 보스용 임시 사각형
    QUERY_DRAW tmp_boss = {
        .x  = bossRuntime.x,
        .y  = bossRuntime.y,
        .w  = bossRuntime.w,
        .h  = bossRuntime.h,
        .ci = bossRuntime.ci
    };

    // a) 플레이어 충돌 → 게임 오버
    if (Collides(&player,tmp_boss.x, tmp_boss.y,tmp_boss.w, tmp_boss.h))
    {
        game_over = GAME_OVER;
    }

    if(tmp_boss.x < X_MAX / 3 && tmp_boss.y < 20 && currentStage == 5){
        Lcd_Printf(0, 10, YELLOW, BACK_COLOR, 1, 1,"Score: %04d", score);
    }

    for (i = 0; i < MAX_BULLET; i++) {
        if (bullet[i].dir == 0) continue;

        if(Collides(&bullet[i],tmp_boss.x, tmp_boss.y,tmp_boss.w, tmp_boss.h)){
            bossRuntime.hp--;
            platforms[5].w -= X_MAX/BOSS_HP;
            platforms[10].x -= X_MAX/BOSS_HP;
            platforms[10].w = platforms[10].x;
            if(bossRuntime.hp == 0){
                if(currentStage == 3){
                    platforms[5].w = 0;
                    platforms[10].x = 0;
                    platforms[10].w = platforms[10].x;
                    clear = 1;
                }
                else{
                    platforms[5].w = X_MAX;
                    platforms[10].x = X_MAX;
                    platforms[10].w = platforms[10].x;
                    bossRuntime.hp = BOSS_HP;
                    score += 100;
                }
            }

            if(currentStage == 5){
                score +=10;
                Lcd_Printf(0, 10, BACK_COLOR, BACK_COLOR, 1, 1, "          ");
                Lcd_Printf(0, 10, YELLOW, BACK_COLOR, 1, 1,"Score: %04d", score);
            }
            bossRuntime.ci = 0;
            Draw_Boss(&bossRuntime);
            bullet[i].ci = BLACK;
            Draw_Object(&bullet[i]);
            bullet[i].dir = 0;
            goto next;
            
        }
    }
    // —————— 그리기 ——————
 
    Draw_Boss(&bossRuntime);
    next:
    for (i = 0; i < bossRuntime.numBullets; i++) {
        ObjDef *bl = &bossRuntime.bullets[i];
        if (bl->item_enable) {
            QUERY_DRAW tmp = { bl->x, bl->y, bl->w, bl->h, bl->ci };
            tmp.ci = BOSS_BL_COLOR;
            Draw_Object(&tmp);
            }
        }
}


static void Game_Init(void) {
    int i;
	Lcd_Clr_Screen();
    player.x = 0; player.y = 200; player.w = PLAYER_SIZE_X; player.h = PLAYER_SIZE_Y; 
    player.ci = PLAYER_COLOR; player.dir = RIGHT; player.is_Jumping = 1; player.vy = 0, player.jumpCount = 0;

    for (i = 0; i < MAX_BULLET; i++) {
        bullet[i].dir = 0;  // 총알 비활성화
    }

    const StageDef *st = &stages[currentStage];
    for (i = 0; i < st->numPlatforms; i++) {
        platforms[i] = (QUERY_DRAW){
            .x = st->platforms[i].x,
            .y = st->platforms[i].y,
            .w = st->platforms[i].w,
            .h = st->platforms[i].h,
            .ci = st->platforms[i].ci,
            .vx = 0, .vy = 0,
        };
        Draw_Object(&platforms[i]);
    }

    platformCount = st->numPlatforms;

    for (i = 0; i < st->numWalls; i++) {
        walls[i] = (QUERY_DRAW){
            .x = st->walls[i].x,
            .y = st->walls[i].y,
            .w = st->walls[i].w,
            .h = st->walls[i].h,
            .ci = st->walls[i].ci,
            .vx = st->walls[i].vx, 
            .vy = st->walls[i].vy,
        };
        Draw_Object(&walls[i]);
    }
    wallCount = st->numWalls;

    for(i = 0; i < st->numitems; i++){
        items[i] = (QUERY_DRAW){
            .x = st->items[i].x,
            .y = st->items[i].y,
            .w = st->items[i].w,
            .h = st->items[i].h,
            .ci = st->items[i].ci,
            .vx = 0, .vy = 0,
            .item_enable = st->items[i].item_enable
        };
        Draw_Object(&items[i]);
    }
    itemCount = st->numitems;

    Lcd_Draw_Box(player.x, player.y, player.w, player.h, color[player.ci]);
}

void Boss_Init_Stage(int stageIdx) {
    int i;
    if(stageIdx < 0 || stageIdx >= sizeof(stages)/sizeof(stages[0])) {
        // 올바른 인덱스 범위 체크
        return;
    }
    bossRuntime = stages[stageIdx].boss;  // 직접 복사 후 초기화
    for (i = 0; i < MAX_BOSS_BULLETS; i++){
        bossRuntime.bullets[i].item_enable = 0;
    }
    bossRuntime.lastFire = ms_tick;
    bossRuntime.currentPattern = 0;
    bossRuntime.patternTimer = 0;
    zig_flag = 0;
    invert_flag = 1;

    // 누락된 초기화 인자 또는 필드 체크
    if(bossRuntime.hp <= 0) bossRuntime.hp = BOSS_HP; // 기본체력 설정
}

extern volatile int TIM4_expired;
extern int volatile TIM3_expired;
extern volatile int USART1_rx_ready;
extern volatile int USART1_rx_data;
extern volatile int Jog_key_in;
extern volatile int Jog_key;
extern int Jog_key_sw_in;
extern volatile int Fall_Count;
extern volatile int TIM2_expired;

static uint32_t lastTogglePlatforms = 0;
static uint32_t lastToggleWalls     = 0;

void System_Init(void) {
    Clock_Init();
    LED_Init();
    Key_Poll_Init();
    Uart1_Init(115200);

    SCB->VTOR = 0x08003000;
    SCB->SHCSR = 7 << 16;
}
#define BASE  (500) //msec

static void Buzzer_Beep(unsigned char tone)
{
  const static unsigned short tone_value[] = {261,277,293,311,329,349,369,391,415,440,466,493,523,554,587,622,659,698,739,783,830,880,932,987};

  TIM3_Out_Freq_Generation(tone_value[tone]);

}
enum key{C1, C1_, D1, D1_, E1, F1, F1_, G1, G1_, A1, A1_, B1, C2, C2_, D2, D2_, E2, F2, F2_, G2, G2_, A2, A2_, B2};
enum note{N16=BASE/4, N8=BASE/2, N4=BASE, N2=BASE*2, N1=BASE*4};
const int song1[][2] ={   
    { A1, N4 }, { E2, N4 }, { A2, N4 }, { E2, N4 },

    { A1, N4 }, { C2, N4 }, { B1, N8 }, { A1, N8 },

    { G1, N4 }, { D2, N4 }, { G2, N4 }, { D2, N4 },

    { G2, N4 }, { F2, N4 }, { E2, N8 }, { D2, N8 },

    { A2, N8 }, { E2, N8 }, { A2, N8 }, { E2, N8 },

    { A1, N2 }
};
const int song2[][2] = {
    { C1,  N2 }, { D1_, N2 }, { G1,  N2 },
    { C1,  N2 }, { D1_, N2 }, { G1,  N2 },

    { A1,  N4 }, { G1,  N4 }, { F1,  N4 }, { D1_, N4 },

    { C1,  N2 }, { G1,  N2 }, { C2,  N2 },

    { G1,  N1 }
};

  const char * note_name[] = {"C1", "C1#", "D1", "D1#", "E1", "F1", "F1#", "G1", "G1#", "A1", "A1#", "B1", "C2", "C2#", "D2", "D2#", "E2", "F2", "F2#", "G2", "G2#", "A2", "A2#", "B2"};

#define DISPLAY_MODE        3

void Main(void) {
    System_Init();
    Lcd_Init(DISPLAY_MODE);
    Jog_Poll_Init();
    Jog_ISR_Enable(1);
    Uart1_RX_Interrupt_Enable(1);
    ms_tick = 0;
    SysTick_Run(1);
    
    for (;;) {
        if(currentStage == 4){
            TIM3_Repeat_Interrupt_Enable(0, 0);
            TIM2_Repeat_Interrupt_Enable(0,0);
            TIM3_Out_Stop();
            Lcd_Printf(X_MAX/2 - 40,Y_MAX/2 - 80,MAGENTA,BACK_COLOR,2,2,"CLEAR!");
            Lcd_Printf(X_MAX/2 - 95,Y_MAX/2 - 30,MAGENTA,BACK_COLOR,2,2,"You are Hero!");
            Lcd_Printf(X_MAX/2 - 100,Y_MAX/2 + 40,YELLOW,BACK_COLOR,1,1,"You can play the SCORE GAME");
            Lcd_Printf(X_MAX/2 - 70,Y_MAX/2 + 60,YELLOW,BACK_COLOR,1,1,"by pressing any key");
            Jog_Wait_Key_Pressed();
            Jog_Wait_Key_Released();
            currentStage++;
        }
        Game_Init();
        Boss_Init_Stage(currentStage);
        TIM4_Repeat_Interrupt_Enable(1, TIMER4_PERIOD);
        ms_tick = 0;
        // 게임 시작 전 한 번만 리셋
        score = 0;
        game_over = 0;
        Jog_key = 0;
        Wall2_Flag = 0;
        int i = 0, j = 1;
        if(currentStage == 5) Lcd_Printf(0, 10, YELLOW, BACK_COLOR, 1, 1,"Score: %04d", score);

        TIM3_Out_Init();
        if(currentStage < 3){
            TIM2_Repeat_Interrupt_Enable(1,song1[i][1]);
            Buzzer_Beep(song1[i][0]);
            TIM2_Change_Value(song1[j][1]);
        }
        else if(currentStage != 4){
            TIM2_Repeat_Interrupt_Enable(1,song2[i][1]);
            Buzzer_Beep(song2[i][0]);
            TIM2_Change_Value(song2[j][1]);
        }
        // 이 while이 game_over 플래그가 1이 될 때까지 돈다
        if(currentStage != 4){
            while (!game_over) {
                player.vx = 0;
                if (Jog_key_in && TIM4_expired) {
                    if (Jog_key < 4) {
                        player.ci = BACK_COLOR;
                        Draw_Object(&player);
                        Player_Move(Jog_key);
                        player.ci = PLAYER_COLOR;
                        Draw_Object(&player);
                        TIM4_expired = 0;
                        if (Jog_Get_Pressed() == 0) 
                            Jog_key_in = 0;
                    }
                }

                if (Jog_key_sw_in) {
                    if (Jog_key == 4) {
                        k4();
                        Jog_key_sw_in = 0;
                    } 
                    else{
                        k5();
                        Jog_key_sw_in = 0;
                    }
                }
                
                if (ms_tick % 50 == 0) {
                    Bullet_Move();
                    Boss_Move();
                    
                    player.ci = BACK_COLOR;
                    Draw_Object(&player);
                    Apply_Gravity();
                    if(game_over) break;
                    player.ci = PLAYER_COLOR;
                    Draw_Object(&player);

                    if(currentStage == 0){
                        if(ms_tick - lastTogglePlatforms >= 2000) {
                            Wall1_Flag = 1;
                            lastTogglePlatforms = ms_tick;
                        }
                        if(player.y <= walls[2].y && Wall2_Flag == 0) Wall2_Flag = 1;
                        if(Wall1_Flag == 1){
                            if(MoveWall_y(1) == 0){
                                Wall1_Flag = 0;
                            }
                        }
                        if(Wall2_Flag == 1) MoveWall_x(2);
                    }
                }
                if(TIM2_expired){
                    i++, j++;
                    if(currentStage < 3){
                        if(i == (sizeof(song1)/sizeof(song1[0]))-1) j = 0;
                        TIM2_Change_Value(song1[j][1]);
                        Buzzer_Beep(song1[i][0]);
                        if(i == (sizeof(song1)/sizeof(song1[0]))-1) i = -1;
                    }
                    else if(currentStage != 4){
                        if(i == (sizeof(song2)/sizeof(song2[0]))-1) j = 0;
                        TIM2_Change_Value(song2[j][1]);
                        Buzzer_Beep(song2[i][0]);
                        if(i == (sizeof(song2)/sizeof(song2[0]))-1) i = -1;
                    }
                    TIM2_expired = 0;
                }

                if(currentStage == 1){
                    if (ms_tick - lastTogglePlatforms >= 1300) {
                        Toggle_Platforms();
                        lastTogglePlatforms = ms_tick;
                    }
                
                }

                if(currentStage == 2){
                        // 벽: 500ms 간격
                    if (ms_tick - lastToggleWalls >= 500) {
                        Toggle_Walls();
                        lastToggleWalls = ms_tick;
                    }
                }

                if (clear) {
                    currentStage++;
                    clear = 0;
                    Game_Init();
                    Boss_Init_Stage(currentStage);
                    break;
                }
                
            }
            if(game_over == 1){
                // --- Game Over 처리 ---
                TIM4_Repeat_Interrupt_Enable(0, 0);
                TIM3_Repeat_Interrupt_Enable(0, 0);
                TIM2_Repeat_Interrupt_Enable(0,0);
                if(currentStage != 5){
                    Lcd_Printf(X_MAX/2 - 62,Y_MAX/2 - 70,RED,BACK_COLOR,2,2,"YOU DIED");
                    Lcd_Printf(X_MAX/2 - 75,Y_MAX/2,RED,BACK_COLOR,1,1,"Please press any key");
                }
                else {
                    Lcd_Printf(X_MAX/2 - 85, Y_MAX/2 - 70, YELLOW, BACK_COLOR, 2, 2,"Score: %04d", score);
                    Lcd_Printf(X_MAX/2 - 75,Y_MAX/2,WHITE,BACK_COLOR,1,1,"Please press any key");
                }
                Uart_Printf("Game Over, Please press any key to continue.\n");
                Jog_Wait_Key_Pressed();
                Jog_Wait_Key_Released();
                Uart_Printf("Game Start\n");
            }
            

            // for(;;) 루프로 돌아가면서 Game_Init()부터 새로 시작
        }
    }
    
}
