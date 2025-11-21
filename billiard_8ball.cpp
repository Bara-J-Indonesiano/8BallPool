// g++ billiard_8ball.cpp -o billiard -lraylib -lm -lpthread -ldl -lrt -lGL
// ./billiard

#include "raylib.h"
#include <vector>
#include <cmath>
#include <string>
#include <algorithm>

float clampf(float v, float lo, float hi) {
    return fmaxf(lo, fminf(v, hi));
}

float Distance(Vector2 a, Vector2 b) {
    return sqrtf((a.x-b.x)*(a.x-b.x) + (a.y-b.y)*(a.y-b.y));
}


struct Ball {
    Vector2 pos;
    Vector2 vel;
    bool active;
    Color color;
    int id;
};


const int SCREEN_W = 1000;
const int SCREEN_H = 650;

const Rectangle TABLE = {80, 80, 840, 490};
const float BALL_RADIUS = 12.0f;
const float HOLE_RADIUS = 26.0f;

const float FRICTION = 0.994f;
const float MIN_VELOCITY = 0.03f;

void ResolveBallCollision(Ball &a, Ball &b) {
    if (!a.active || !b.active) return;

    Vector2 n = {b.pos.x - a.pos.x, b.pos.y - a.pos.y};
    float dist = sqrtf(n.x*n.x + n.y*n.y);
    if (dist <= 0 || dist >= 2*BALL_RADIUS) return;

    float overlap = 2*BALL_RADIUS - dist;
    Vector2 n_norm = {n.x / dist, n.y / dist};

    a.pos.x -= n_norm.x * overlap * 0.5f;
    a.pos.y -= n_norm.y * overlap * 0.5f;
    b.pos.x += n_norm.x * overlap * 0.5f;
    b.pos.y += n_norm.y * overlap * 0.5f;

    Vector2 rv = {b.vel.x - a.vel.x, b.vel.y - a.vel.y};
    float velAlongNormal = rv.x*n_norm.x + rv.y*n_norm.y;
    if (velAlongNormal > 0) return;

    float j = -(1 + 0.98f) * velAlongNormal;
    j /= 2.0f;

    Vector2 impulse = {j*n_norm.x, j*n_norm.y};
    a.vel.x -= impulse.x;
    a.vel.y -= impulse.y;
    b.vel.x += impulse.x;
    b.vel.y += impulse.y;
}

std::vector<Vector2> CreateRackPositions(Vector2 rackStart) {
    std::vector<Vector2> pos;
    float sep = 2*BALL_RADIUS + 1.5f;

    for(int r=0;r<5;r++){
        float x = rackStart.x + r*sep;
        float y = rackStart.y - (r*sep)/2;
        for(int i=0;i<=r;i++){
            pos.push_back({x, y + i*sep});
        }
    }
    return pos;
}

void DrawDashedLine(Vector2 start, Vector2 end, float dashLength, float gapLength, Color color) {
    float length = Distance(start, end);
    Vector2 dir = {(end.x - start.x)/length, (end.y - start.y)/length};

    float progress = 0;
    while(progress < length) {
        Vector2 a = { start.x + dir.x * progress,
                      start.y + dir.y * progress };
        Vector2 b = { start.x + dir.x * fminf(progress + dashLength, length),
                      start.y + dir.y * fminf(progress + dashLength, length) };

        DrawLineV(a, b, color);
        progress += dashLength + gapLength;
    }
}

void DrawSkinnedBall(Vector2 pos, float radius, Color base, int id) {
    DrawCircleV(pos, radius, base);

    DrawCircleV({pos.x - radius*0.4f, pos.y - radius*0.4f},
                radius * 0.35f,
                (Color){255,255,255,80});

    DrawCircleV(pos, radius*0.55f, WHITE);

    DrawText(TextFormat("%d", id),
             pos.x - radius*0.35f,
             pos.y - radius*0.55f,
             radius,
             BLACK);

    if(id >= 9 && id <= 15){
        DrawRectangle(pos.x - radius,
                      pos.y - radius*0.45f,
                      radius*2,
                      radius*0.9f,
                      WHITE);
    }
}

void DrawTableSkinned(Rectangle table) {
    DrawRectangle(table.x-35, table.y-35, table.width+70, 35, (Color){80,40,10,255});
    DrawRectangle(table.x-35, table.y+table.height, table.width+70, 35, (Color){80,40,10,255});
    DrawRectangle(table.x-35, table.y-35, 35, table.height+70, (Color){80,40,10,255});
    DrawRectangle(table.x+table.width, table.y-35, 35, table.height+70, (Color){80,40,10,255});

    Color cloth = (Color){10,120,60,255};
    DrawRectangleRec(table, cloth);

    for(int y = table.y; y < table.y + table.height; y += 6)
        DrawLine(table.x, y, table.x + table.width, y, (Color){0,60,30,25});

    DrawRectangleGradientV(table.x, table.y, table.width, 20,
        (Color){0,0,0,110}, (Color){0,0,0,0});
    DrawRectangleGradientV(table.x, table.y+table.height-20, table.width, 20,
        (Color){0,0,0,0}, (Color){0,0,0,110});
}

void DrawCueStick(Vector2 start, Vector2 end) {
    float thickness = 10.0f;

    DrawLineEx(start, end, thickness, (Color){181, 101, 29, 255});

    Vector2 gripStart = {
        end.x + (start.x - end.x)*0.25f,
        end.y + (start.y - end.y)*0.25f
    };
    DrawLineEx(gripStart, end, thickness, BLACK);

    Vector2 ferruleStart = {
        start.x + (end.x - start.x)*0.08f,
        start.y + (end.y - start.y)*0.08f
    };
    DrawLineEx(start, ferruleStart, thickness, (Color){245,245,220,255});

    DrawCircleV(start, thickness/2, (Color){50,120,255,255});
}

int main() {
    InitWindow(SCREEN_W, SCREEN_H, "8 Ball Pool - Raylib Enhanced Skin");
    SetTargetFPS(60);

    std::vector<Vector2> holes = {
        {TABLE.x - 8, TABLE.y - 8},
        {TABLE.x + TABLE.width/2.0f, TABLE.y - 8},
        {TABLE.x + TABLE.width + 8, TABLE.y - 8},
        {TABLE.x - 8, TABLE.y + TABLE.height + 8},
        {TABLE.x + TABLE.width/2.0f, TABLE.y + TABLE.height + 8},
        {TABLE.x + TABLE.width + 8, TABLE.y + TABLE.height + 8}
    };

    auto ballColor = [&](int id)->Color {
        if (id == 0) return WHITE;
        if (id == 8) return BLACK;
        Color tbl[] = {RED, ORANGE, GOLD, BLUE, PURPLE, DARKGREEN, MAROON};
        if(id>=1 && id<=7) return tbl[id-1];
        if(id>=9 && id<=15) return tbl[id-9];
        return WHITE;
    };

    auto initBalls = [&](){
        std::vector<Ball> b;
        b.push_back({{TABLE.x + 140, TABLE.y + TABLE.height/2}, {0,0}, true, WHITE, 0});

        Vector2 rackTip = {TABLE.x + TABLE.width - 160, TABLE.y + TABLE.height/2};
        auto rack = CreateRackPositions(rackTip);

        std::vector<int> order = {
            1,
            15,2,
            9,8,3,
            10,4,11,5,
            12,6,13,7,14
        };

        for(int i=0;i<15;i++){
            int id = order[i];
            b.push_back({rack[i], {0,0}, true, ballColor(id), id});
        }
        return b;
    };

    auto balls = initBalls();

    int currentPlayer = 1;
    int score[3] = {0,0,0};

    bool waitingPlacement = false;
    bool ballInHand = false;
    bool shotInProgress = false;
    bool charging = false;

    float power = 0;
    const float MAX_POWER = 20;

    bool gameOver = false;
    int winner = -1;

    while(!WindowShouldClose()){

        Vector2 mouse = GetMousePosition();
        Vector2 cuePos = balls[0].pos;
        float angle = atan2f(mouse.y - cuePos.y, mouse.x - cuePos.x);

        if(waitingPlacement && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)){
            Vector2 p = mouse;
            p.x = clampf(p.x, TABLE.x+BALL_RADIUS, TABLE.x+TABLE.width-BALL_RADIUS);
            p.y = clampf(p.y, TABLE.y+BALL_RADIUS, TABLE.y+TABLE.height-BALL_RADIUS);

            bool ok = true;
            for(size_t i=1;i<balls.size();i++){
                if(balls[i].active && Distance(p, balls[i].pos) < 2*BALL_RADIUS+1){
                    ok=false;
                    break;
                }
            }
            if(ok){
                balls[0].pos = p;
                balls[0].vel = {0,0};
                waitingPlacement=false;
                ballInHand=false;
            }
        }


        if(!shotInProgress && !waitingPlacement && !gameOver){
            if(IsMouseButtonDown(MOUSE_LEFT_BUTTON)){
                charging = true;
                power += 0.4f;
                if(power>MAX_POWER) power=MAX_POWER;
            }
            if(IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && charging){
                balls[0].vel.x = cosf(angle)*(-power);
                balls[0].vel.y = sinf(angle)*(-power);
                shotInProgress = true;
                charging = false;
                power=0;
            }
        }

        for(auto &b : balls){
            if(!b.active) continue;

            b.pos.x += b.vel.x;
            b.pos.y += b.vel.y;

            b.vel.x *= FRICTION;
            b.vel.y *= FRICTION;

            if(fabs(b.vel.x)<MIN_VELOCITY) b.vel.x=0;
            if(fabs(b.vel.y)<MIN_VELOCITY) b.vel.y=0;

            float L = TABLE.x + 14;
            float R = TABLE.x + TABLE.width - 14;
            float T = TABLE.y + 14;
            float B = TABLE.y + TABLE.height - 14;

            bool nearPocket=false;
            for(auto &h : holes)
                if(Distance(b.pos,h) < HOLE_RADIUS+18)
                    nearPocket=true;

            if(!nearPocket){
                if(b.pos.x<L){ b.pos.x=L; b.vel.x*=-1; }
                if(b.pos.x>R){ b.pos.x=R; b.vel.x*=-1; }
                if(b.pos.y<T){ b.pos.y=T; b.vel.y*=-1; }
                if(b.pos.y>B){ b.pos.y=B; b.vel.y*=-1; }
            }
        }

        for(size_t i=0;i<balls.size();i++)
            for(size_t j=i+1;j<balls.size();j++)
                ResolveBallCollision(balls[i], balls[j]);

        std::vector<int> pocketed;
        for(auto &b : balls){
            if(!b.active) continue;
            for(auto &h:holes){
                if(Distance(b.pos,h) < HOLE_RADIUS-4){
                    pocketed.push_back(b.id);
                    if(b.id != 0) b.active=false;
                }
            }
        }

        bool foul=false;
        bool scoredBall=false;
        bool pocket8=false;

        for(int id : pocketed){
            if(id==0) foul=true;
            else if(id==8) pocket8=true;
            else {
                score[currentPlayer]++;
                scoredBall=true;
            }
        }

        if(foul){
            score[currentPlayer] = std::max(0, score[currentPlayer]-1);
            balls[0].pos = {TABLE.x+140, TABLE.y+TABLE.height/2};
            balls[0].vel = {0,0};
            currentPlayer = (currentPlayer==1?2:1);
            waitingPlacement=true;
            ballInHand=true;
            shotInProgress=false;
        }

        if(pocket8){
            winner=currentPlayer;
            gameOver=true;
        }

        bool moving=false;
        for(auto &b : balls)
            if(b.active && (fabs(b.vel.x)>MIN_VELOCITY || fabs(b.vel.y)>MIN_VELOCITY))
                moving=true;

        if(!moving && shotInProgress){
            if(!scoredBall && !foul)
                currentPlayer=(currentPlayer==1?2:1);

            shotInProgress=false;
        }

        BeginDrawing();
        ClearBackground(DARKGREEN);

        DrawTableSkinned(TABLE);

        for(auto &h : holes)
            DrawCircleV(h, HOLE_RADIUS, BLACK);

        for(auto &b : balls){
            if(!b.active) continue;
            DrawSkinnedBall(b.pos, BALL_RADIUS, b.color, b.id);
        }

        if(!shotInProgress && !waitingPlacement && !gameOver){
            Vector2 cueFront = {
                cuePos.x + cosf(angle)*180,
                cuePos.y + sinf(angle)*180
            };
            DrawCueStick(cuePos, cueFront);

            Vector2 trajBack = {
                cuePos.x - cosf(angle)*500,
                cuePos.y - sinf(angle)*500
            };
            DrawDashedLine(cuePos, trajBack, 8, 6, WHITE);
        }

        // UI
        DrawText(TextFormat("Power:"), 24, 22, 18, WHITE);
        DrawRectangle(110,20,300,18,LIGHTGRAY);
        DrawRectangle(110,20,(int)((power/MAX_POWER)*300),18,ORANGE);

        DrawText(TextFormat("Turn: Player %d", currentPlayer), 500, 20, 24, YELLOW);

        DrawText(TextFormat("P1 Score: %d", score[1]), 24, SCREEN_H-80, 22, WHITE);
        DrawText(TextFormat("P2 Score: %d", score[2]), 24, SCREEN_H-50, 22, WHITE);

        if(waitingPlacement)
            DrawText("Place cue ball: Click inside table", 530, 20, 18, GOLD);

        if(gameOver){
            DrawText(TextFormat("GAME OVER! Winner: Player %d", winner),
                     SCREEN_W/2-180, SCREEN_H/2-40, 30, GOLD);
            DrawText("Press R to Restart", SCREEN_W/2-80, SCREEN_H/2, 20, WHITE);
        }

        EndDrawing();

        // Restart
        if(IsKeyPressed(KEY_R)){
            balls=initBalls();
            score[1]=score[2]=0;
            currentPlayer=1;
            waitingPlacement=false;
            ballInHand=false;
            shotInProgress=false;
            power=0;
            charging=false;
            gameOver=false;
            winner=-1;
        }
    }

    CloseWindow();
    return 0;
}
