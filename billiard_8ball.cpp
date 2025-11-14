// billiard_8ball.cpp
// Compile on Ubuntu:
// sudo apt install libraylib-dev g++
// g++ billiard_8ball.cpp -o billiard -lraylib -lm -lpthread -ldl -lrt -lGL
// ./billiard

#include "raylib.h"
#include <vector>
#include <cmath>
#include <string>
#include <algorithm>

struct Ball {
    Vector2 pos;
    Vector2 vel;
    bool active;
    Color color;
    int id; // 0 = cue, 1-7 solids, 8 eight-ball, 9-15 stripes
};

const int SCREEN_W = 1000;
const int SCREEN_H = 650;

const Rectangle TABLE = {80, 80, 840, 490};
const float BALL_RADIUS = 12.0f;
const float HOLE_RADIUS = 20.0f;
const float FRICTION = 0.994f;
const float MIN_VELOCITY = 0.03f;

float Distance(const Vector2 &a, const Vector2 &b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return sqrtf(dx*dx + dy*dy);
}

Vector2 Normalize(const Vector2 &v) {
    float len = sqrtf(v.x*v.x + v.y*v.y);
    if (len == 0) return {0,0};
    return {v.x/len, v.y/len};
}

void ResolveBallCollision(Ball &a, Ball &b) {
    if (!a.active || !b.active) return;

    Vector2 n = {b.pos.x - a.pos.x, b.pos.y - a.pos.y};
    float dist = sqrtf(n.x*n.x + n.y*n.y);
    if (dist <= 0.0f) return;
    if (dist >= 2*BALL_RADIUS) return;

    float overlap = 2*BALL_RADIUS - dist;
    Vector2 n_norm = {n.x / dist, n.y / dist};
    a.pos.x -= n_norm.x * overlap * 0.5f;
    a.pos.y -= n_norm.y * overlap * 0.5f;
    b.pos.x += n_norm.x * overlap * 0.5f;
    b.pos.y += n_norm.y * overlap * 0.5f;

    Vector2 rv = {b.vel.x - a.vel.x, b.vel.y - a.vel.y};
    float velAlongNormal = rv.x * n_norm.x + rv.y * n_norm.y;
    if (velAlongNormal > 0) return;

    const float e = 0.98f;
    float j = -(1 + e) * velAlongNormal;
    j /= 2.0f;

    Vector2 impulse = {j * n_norm.x, j * n_norm.y};
    a.vel.x -= impulse.x;
    a.vel.y -= impulse.y;
    b.vel.x += impulse.x;
    b.vel.y += impulse.y;
}

std::vector<Vector2> CreateRackPositions(Vector2 rackStart) {
    std::vector<Vector2> pos;
    float sep = BALL_RADIUS * 2 + 1.5f;

    for (int row = 0; row < 5; ++row) {
        float startX = rackStart.x + row * sep;
        float startY = rackStart.y - (row * sep / 2.0f);
        for (int i = 0; i <= row; ++i) {
            pos.push_back({ startX, startY + i * sep });
        }
    }
    return pos; // 15 positions
}

int main() {
    InitWindow(SCREEN_W, SCREEN_H, "8 Ball Pool (Raylib, 15 Balls)");
    SetTargetFPS(60);

    // pockets
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
        Color base[] = { RED, ORANGE, GOLD, BLUE, PURPLE, DARKGREEN, MAROON };
        if (id >= 1 && id <= 7) return base[(id-1)%7];
        if (id >= 9 && id <= 15) return base[(id-9)%7];
        return WHITE;
    };

    auto initBalls = [&]()->std::vector<Ball> {
        std::vector<Ball> b;

        // Cue ball
        b.push_back({ {TABLE.x + 140, TABLE.y + TABLE.height/2.0f}, {0,0}, true, WHITE, 0 });

        Vector2 rackTip = {TABLE.x + TABLE.width - 160, TABLE.y + TABLE.height/2.0f};
        auto pos = CreateRackPositions(rackTip);

        std::vector<int> order = {
            1,
            15, 2,
            9, 8, 3,
            10, 4, 11, 5,
            12, 6, 13, 7, 14
        };

        for (size_t i=0; i<pos.size(); ++i) {
            int id = order[i];
            b.push_back({pos[i], {0,0}, true, ballColor(id), id});
        }
        return b;
    };

    std::vector<Ball> balls = initBalls();

    int currentPlayer = 1;
    int playerGroup[3] = {0, 0, 0}; // 0 unassigned, 1 solids, 2 stripes
    int scoreP[3] = {0,0,0};

    bool waitingPlacement = false;
    bool ballInHand = false;
    bool shotInProgress = false;
    bool charging = false;

    float power = 0;
    const float MAX_POWER = 20;

    bool gameOver = false;
    int winner = -1;

    SetMouseCursor(MOUSE_CURSOR_CROSSHAIR);

    while (!WindowShouldClose()) {
        Vector2 mouse = GetMousePosition();
        Ball &cue = balls[0];
        float angle = atan2f(mouse.y - cue.pos.y, mouse.x - cue.pos.x);

        // ball in hand placement
        if (waitingPlacement && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            Vector2 p = mouse;
            p.x = std::clamp(p.x, TABLE.x + BALL_RADIUS, TABLE.x + TABLE.width - BALL_RADIUS);
            p.y = std::clamp(p.y, TABLE.y + BALL_RADIUS, TABLE.y + TABLE.height - BALL_RADIUS);

            bool ok = true;
            for (size_t i=1;i<balls.size();++i)
                if (balls[i].active && Distance(p, balls[i].pos) < 2*BALL_RADIUS+2)
                    ok = false;

            if (ok) {
                cue.pos = p;
                cue.vel = {0,0};
                waitingPlacement = false;
                ballInHand = false;
            }
        }

        // shooting input
        if (!shotInProgress && !waitingPlacement && !gameOver) {
            if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                charging = true;
                power += 0.4f;
                if (power > MAX_POWER) power = MAX_POWER;
            }
            if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && charging) {
                cue.vel.x = cosf(angle) * -power;
                cue.vel.y = sinf(angle) * -power;
                charging = false;
                shotInProgress = true;
                power = 0;
            }
        }

        // move balls
        for (auto &b : balls) {
            if (!b.active) continue;

            b.pos.x += b.vel.x;
            b.pos.y += b.vel.y;
            b.vel.x *= FRICTION;
            b.vel.y *= FRICTION;

            if (fabs(b.vel.x) < MIN_VELOCITY) b.vel.x = 0;
            if (fabs(b.vel.y) < MIN_VELOCITY) b.vel.y = 0;

            float L = TABLE.x + 14;
            float R = TABLE.x + TABLE.width - 14;
            float T = TABLE.y + 14;
            float B = TABLE.y + TABLE.height - 14;

            if (b.pos.x < L) { b.pos.x = L; b.vel.x *= -1; }
            if (b.pos.x > R) { b.pos.x = R; b.vel.x *= -1; }
            if (b.pos.y < T) { b.pos.y = T; b.vel.y *= -1; }
            if (b.pos.y > B) { b.pos.y = B; b.vel.y *= -1; }
        }

        // collisions
        for (size_t i=0;i<balls.size();++i)
        for (size_t j=i+1;j<balls.size();++j)
            ResolveBallCollision(balls[i], balls[j]);

        // pockets
        std::vector<int> pocketed;
        for (auto &b : balls) {
            if (!b.active) continue;
            for (auto &h : holes) {
                if (Distance(b.pos, h) < HOLE_RADIUS) {
                    pocketed.push_back(b.id);
                    if (b.id != 0) b.active = false;
                }
            }
        }

        bool foul = false;
        bool pocketOwn = false;
        bool pocketOpp = false;
        bool pocket8 = false;

        for (int id : pocketed) {
            if (id == 0) foul = true;
            else if (id == 8) pocket8 = true;
            else {
                bool isSolid = (id >=1 && id <=7);
                bool isStripe = (id >=9 && id <=15);

                if (playerGroup[currentPlayer] == 0) {
                    playerGroup[currentPlayer] = isSolid ? 1 : 2;
                    int op = currentPlayer==1?2:1;
                    playerGroup[op] = playerGroup[currentPlayer]==1?2:1;
                }

                int g = playerGroup[currentPlayer];
                bool isMine = (g==1 && isSolid) || (g==2 && isStripe);

                if (isMine) {
                    pocketOwn = true;
                    scoreP[currentPlayer]++;
                } else {
                    pocketOpp = true;
                }
            }
        }

        // foul handling
        if (foul) {
            scoreP[currentPlayer] = std::max(0, scoreP[currentPlayer] - 1);
            balls[0].pos = {TABLE.x + 140, TABLE.y + TABLE.height/2};
            balls[0].vel = {0,0};
            currentPlayer = currentPlayer==1?2:1;
            ballInHand = true;
            waitingPlacement = true;
            shotInProgress = false;
        }

        // 8 ball
        if (pocket8) {
            int need = 0;
            for (auto &b : balls) {
                if (!b.active) continue;
                if (b.id==0 || b.id==8) continue;

                bool isSolid = b.id>=1 && b.id<=7;
                bool isStripe = b.id>=9 && b.id<=15;

                int g = playerGroup[currentPlayer];
                if ((g==1 && isSolid) || (g==2 && isStripe))
                    need++;
            }

            if (need==0) {
                winner = currentPlayer;
                gameOver = true;
            } else {
                winner = currentPlayer==1?2:1;
                gameOver = true;
            }
            shotInProgress = false;
        }

        // check if all balls stopped
        bool anyMoving = false;
        for (auto &b : balls) {
            if (!b.active) continue;
            if (fabs(b.vel.x) > MIN_VELOCITY || fabs(b.vel.y) > MIN_VELOCITY)
                anyMoving = true;
        }

        if (!anyMoving && shotInProgress) {
            if (!foul) {
                if (pocketOwn) {} 
                else currentPlayer = (currentPlayer==1?2:1);
            }
            shotInProgress = false;
        }

        // DRAW --------------------------------------------------------------
        BeginDrawing();
        ClearBackground(DARKGREEN);

        DrawRectangle(TABLE.x-24, TABLE.y-24, TABLE.width+48, 24, BROWN);
        DrawRectangle(TABLE.x-24, TABLE.y+TABLE.height, TABLE.width+48, 24, BROWN);
        DrawRectangle(TABLE.x-24, TABLE.y-24, 24, TABLE.height+48, BROWN);
        DrawRectangle(TABLE.x+TABLE.width, TABLE.y-24, 24, TABLE.height+48, BROWN);

        DrawRectangleRec(TABLE, GREEN);

        for (auto &h : holes) DrawCircleV(h, HOLE_RADIUS, BLACK);

        // balls
        for (auto &b : balls) {
            if (!b.active) continue;
            DrawCircleV(b.pos, BALL_RADIUS, b.color);

            if (b.id>=9 && b.id<=15) {
                DrawCircleV({b.pos.x+4, b.pos.y-2}, BALL_RADIUS-4, RAYWHITE);
                DrawCircleV({b.pos.x+4, b.pos.y-2}, BALL_RADIUS-6, b.color);
            }

            if (b.id == 0) DrawCircleV({b.pos.x-3, b.pos.y-3}, 3, GRAY);
        }

        // cue stick
        if (!shotInProgress && !waitingPlacement && !gameOver) {
            Vector2 end = {
                cue.pos.x - cosf(angle)*(60 + power*3.0f),
                cue.pos.y - sinf(angle)*(60 + power*3.0f)
            };
            DrawLineEx(cue.pos, end, 6, BEIGE);
        }

        // power bar
        DrawText("Power:", 24, 22, 18, RAYWHITE);
        DrawRectangle(110, 20, 300, 18, LIGHTGRAY);
        DrawRectangle(110, 20, (int)((power/MAX_POWER)*300), 18, ORANGE);

        // UI
        DrawText(TextFormat("Turn: Player %d", currentPlayer), 400, 12, 22, YELLOW);

        DrawText(TextFormat("P1 Score: %d", scoreP[1]), 24, SCREEN_H-80, 20, WHITE);
        DrawText(TextFormat("P2 Score: %d", scoreP[2]), 24, SCREEN_H-50, 20, WHITE);

        const char* gname[] = {"Unassigned","Solids","Stripes"};
        DrawText(TextFormat("P1 Group: %s", gname[playerGroup[1]]), 200, SCREEN_H-80, 18, WHITE);
        DrawText(TextFormat("P2 Group: %s", gname[playerGroup[2]]), 200, SCREEN_H-50, 18, WHITE);

        if (waitingPlacement)
            DrawText("Ball in hand: Click on table to place cue ball.", 550, 20, 18, GOLD);

        if (gameOver) {
            DrawText(TextFormat("GAME OVER! Winner: Player %d", winner),
                     SCREEN_W/2 - 180, SCREEN_H/2 - 20, 28, GOLD);
            DrawText("Press R to Restart", SCREEN_W/2 - 90, SCREEN_H/2 + 20, 20, WHITE);
        }

        EndDrawing();

        // restart
        if (IsKeyPressed(KEY_R)) {
            balls = initBalls();
            playerGroup[1] = playerGroup[2] = 0;
            scoreP[1] = scoreP[2] = 0;
            currentPlayer = 1;
            waitingPlacement = false;
            ballInHand = false;
            shotInProgress = false;
            power = 0;
            charging = false;
            gameOver = false;
            winner = -1;
        }
    }

    CloseWindow();
    return 0;
}
