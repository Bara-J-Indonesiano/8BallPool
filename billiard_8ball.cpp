// sudo apt install libraylib-dev g++
// g++ billiard_8ball.cpp -o billiard -lraylib -lm -lpthread -ldl -lrt -lGL
// ./billiard

#include "raylib.h"
#include <vector>
#include <cmath>
#include <string>
#include <algorithm>

static float clampf_custom(float v, float lo, float hi) { return fmaxf(lo, fminf(v, hi)); }
static float Dist(const Vector2 &a,const Vector2 &b){ float dx=a.x-b.x, dy=a.y-b.y; return sqrtf(dx*dx+dy*dy); }
static float Dot(const Vector2 &a,const Vector2 &b){ return a.x*b.x + a.y*b.y; }

struct Segment { Vector2 a,b; };

// Closest point on segment to p
static Vector2 ClosestPointOnSegment(const Vector2 &a, const Vector2 &b, const Vector2 &p, float &tOut) {
    Vector2 ab = {b.x-a.x, b.y-a.y};
    Vector2 ap = {p.x-a.x, p.y-a.y};
    float ab2 = ab.x*ab.x + ab.y*ab.y;
    if (ab2 <= 1e-8f) { tOut = 0.0f; return a; }
    float t = Dot(ap, ab) / ab2;
    tOut = clampf_custom(t, 0.0f, 1.0f);
    return { a.x + ab.x * tOut, a.y + ab.y * tOut };
}

// Ray-ball sampling - returns first hit along ray (step sampling)
static bool RayBallHit(const Vector2 &rayStart, const Vector2 &dirNorm, const Vector2 &ballPos, float maxDist, float ballRadius, Vector2 &outPoint, float &outDist) {
    const float step = fmaxf(3.0f, ballRadius * 0.5f);
    float traveled = 0.0f;
    while (traveled <= maxDist) {
        Vector2 p = { rayStart.x + dirNorm.x * traveled, rayStart.y + dirNorm.y * traveled };
        float d = Dist(p, ballPos);
        if (d <= ballRadius) { outPoint = p; outDist = traveled; return true; }
        traveled += step;
    }
    return false;
}

// Ray-segment sampling - detect near-cushion
static bool RaySegmentHit(const Vector2 &rayStart, const Vector2 &dirNorm, const Segment &seg, float maxDist, float radius, Vector2 &outPoint, float &outDist) {
    const float step = fmaxf(4.0f, radius * 0.6f);
    float traveled = 0.0f;
    while (traveled <= maxDist) {
        Vector2 p = { rayStart.x + dirNorm.x * traveled, rayStart.y + dirNorm.y * traveled };
        float t; Vector2 cp = ClosestPointOnSegment(seg.a, seg.b, p, t);
        float d = Dist(p, cp);
        if (d <= radius) { outPoint = cp; outDist = traveled; return true; }
        traveled += step;
    }
    return false;
}

static Vector2 Reflect(const Vector2 &v, const Vector2 &n) {
    float vn = v.x*n.x + v.y*n.y;
    return { v.x - 2.0f*vn*n.x, v.y - 2.0f*vn*n.y };
}

static void DrawDashedLine(const Vector2 &a, const Vector2 &b, float dash, float gap, Color c) {
    float L = Dist(a,b); if (L <= 1e-6f) return;
    Vector2 dir = { (b.x-a.x)/L, (b.y-a.y)/L };
    float prog = 0.0f;
    while (prog < L) {
        float seg = fminf(dash, L-prog);
        Vector2 p1 = { a.x + dir.x * prog, a.y + dir.y * prog };
        Vector2 p2 = { a.x + dir.x * (prog + seg), a.y + dir.y * (prog + seg) };
        DrawLineV(p1,p2,c);
        prog += dash + gap;
    }
}

// ---------------- Ball & collision ----------------
struct Ball {
    Vector2 pos;
    Vector2 vel;
    bool active;
    Color color;
    int id;
};

static void ResolveBallCollision(Ball &A, Ball &B, float r) {
    if (!A.active || !B.active) return;
    Vector2 n = { B.pos.x - A.pos.x, B.pos.y - A.pos.y };
    float d = sqrtf(n.x*n.x + n.y*n.y);
    if (d <= 1e-6f || d >= 2.0f*r) return;
    Vector2 norm = { n.x/d, n.y/d };
    float overlap = 2.0f*r - d;
    A.pos.x -= norm.x * overlap * 0.5f; A.pos.y -= norm.y * overlap * 0.5f;
    B.pos.x += norm.x * overlap * 0.5f; B.pos.y += norm.y * overlap * 0.5f;
    Vector2 rv = { B.vel.x - A.vel.x, B.vel.y - A.vel.y };
    float velAlongNormal = rv.x*norm.x + rv.y*norm.y;
    if (velAlongNormal > 0) return;
    float e = 0.98f;
    float j = -(1.0f + e) * velAlongNormal;
    j /= 2.0f;
    Vector2 imp = { j*norm.x, j*norm.y };
    A.vel.x -= imp.x; A.vel.y -= imp.y;
    B.vel.x += imp.x; B.vel.y += imp.y;
}

// ---------------- Main ----------------
int main() {
    const int SCREEN_W = 1000;
    const int SCREEN_H = 650;
    InitWindow(SCREEN_W, SCREEN_H, "billiard_8ball (final)");
    SetTargetFPS(60);

    // Table fill most of window - preserve ratio
    const float TABLE_ASPECT = 840.0f/490.0f;
    const float PAD = 40.0f;
    float availW = SCREEN_W - PAD*2;
    float availH = SCREEN_H - PAD*2;
    float tableW = availW;
    float tableH = tableW / TABLE_ASPECT;
    if (tableH > availH) { tableH = availH; tableW = tableH * TABLE_ASPECT; }
    Rectangle TABLE = { (SCREEN_W - tableW)/2.0f, (SCREEN_H - tableH)/2.0f, tableW, tableH };
    const float CUSHION_OFFSET = 14.0f;
    Rectangle play = { TABLE.x + CUSHION_OFFSET, TABLE.y + CUSHION_OFFSET, TABLE.width - 2.0f*CUSHION_OFFSET, TABLE.height - 2.0f*CUSHION_OFFSET };

    // scale derived
    float SCALE = TABLE.width / 840.0f;
    float BALL_R = 12.0f * SCALE;
    float HOLE_R = 26.0f * SCALE;

    // physics params
    float FRICTION = 0.992f;
    const float MIN_VEL = 0.04f;

    // pockets
    std::vector<Vector2> holes = {
        { TABLE.x + HOLE_R*0.7f, TABLE.y + HOLE_R*0.7f },
        { TABLE.x + TABLE.width*0.5f, TABLE.y + HOLE_R*0.7f },
        { TABLE.x + TABLE.width - HOLE_R*0.7f, TABLE.y + HOLE_R*0.7f },
        { TABLE.x + HOLE_R*0.7f, TABLE.y + TABLE.height - HOLE_R*0.7f },
        { TABLE.x + TABLE.width*0.5f, TABLE.y + TABLE.height - HOLE_R*0.7f },
        { TABLE.x + TABLE.width - HOLE_R*0.7f, TABLE.y + TABLE.height - HOLE_R*0.7f }
    };

    // Create cushions (segments) with cutouts for pockets (funnel shape)
    std::vector<Segment> cushions;
    {
        float left = play.x;
        float right = play.x + play.width;
        float top = play.y;
        float bot = play.y + play.height;
        float cut = HOLE_R * 1.2f;

        float pk0x = holes[0].x;
        float pk1x = holes[1].x;
        float pk2x = holes[2].x;
        // between top-left and top-mid
        cushions.push_back({ { pk0x + cut, top }, { pk1x - cut, top }});
        cushions.push_back({ { pk1x + cut, top }, { pk2x - cut, top }});

        // bottom rails mirrored
        float pk3x = holes[3].x;
        float pk4x = holes[4].x;
        float pk5x = holes[5].x;
        cushions.push_back({ { pk3x + cut, bot }, { pk4x - cut, bot }});
        cushions.push_back({ { pk4x + cut, bot }, { pk5x - cut, bot }});
    }

    // load assets from assets/
    std::string baseDir = "assets/";
    Texture2D ballTex[16];
    for (int i=0;i<16;i++) ballTex[i] = {0,0,0,0};
    Texture2D cueTex = {0,0,0,0};
    Font customFont = {0};
    bool texturesOK = true;
    for (int i=0;i<16;i++) {
        std::string fn = baseDir + "ball" + std::to_string(i) + ".png";
        // attempt load (if missing, raylib loads id==0)
        ballTex[i] = LoadTexture(fn.c_str());
    }
    cueTex = LoadTexture((baseDir + "cue.png").c_str());
    customFont = LoadFont((baseDir + "Purisa-BoldOblique.ttf").c_str());
    bool anyBallTex=false;
    for (int i=0;i<16;i++) if (ballTex[i].id != 0) anyBallTex=true;
    if (!(anyBallTex && cueTex.id != 0 && customFont.texture.id != 0)) texturesOK=false;

    auto colorForId = [&](int id)->Color {
        if (id==0) return WHITE;
        if (id==8) return BLACK;
        Color cs[] = { RED, ORANGE, GOLD, BLUE, PURPLE, DARKGREEN, MAROON };
        if (id>=1 && id<=7) return cs[id-1];
        if (id>=9 && id<=15) return cs[(id-9)%7];
        return WHITE;
    };

    // initialize balls: cue on left, rack right
    auto initBalls = [&]()->std::vector<Ball>{
        std::vector<Ball> v;
        v.reserve(16);
        v.push_back({ { play.x + play.width*0.18f, play.y + play.height*0.5f }, {0,0}, true, WHITE, 0 });
        // rack 15
        Vector2 rackTip = { play.x + play.width*0.72f, play.y + play.height*0.5f };
        float sep = (BALL_R*2.0f) + (1.5f * SCALE);
        std::vector<Vector2> pos;
        for (int r=0;r<5;r++){
            float x = rackTip.x + r*sep;
            float y = rackTip.y - (r*sep)/2.0f;
            for (int i=0;i<=r;i++) pos.push_back({ x, y + i*sep });
        }
        std::vector<int> order = { 1, 15, 2, 9, 8, 3, 10, 4, 11, 5, 12, 6, 13, 7, 14 };
        for (int i=0;i<15;i++){
            int id = order[i];
            v.push_back({ pos[i], {0,0}, true, colorForId(id), id });
        }
        return v;
    };

    std::vector<Ball> balls = initBalls();

    // game state
    int currentPlayer = 1;
    int score[3] = {0,0,0};
    bool waitingPlacement = false;
    bool shotInProgress = false;
    bool charging = false;
    float power = 0.0f;
    const float MAX_POWER = 20.0f;
    bool gameOver = false;
    int winner = -1;

    enum GameState { MENU = 0, PLAY = 1, STOPPED = 2 };
    GameState state = MENU;
    // Start center button
    Rectangle btnStart = { SCREEN_W*0.5f - 115.0f, SCREEN_H*0.5f - 36.0f, 230.0f, 72.0f };
    // Stop button bottom-right
    Rectangle btnStop = { SCREEN_W - 150.0f, SCREEN_H - 60.0f, 130.0f, 44.0f };

    int ignoreInputFramesAfterStart = 0;
    const float SLOW_THRESHOLD = 0.08f; // very slow threshold (for early end)
    const float SLOW_DURATION = 0.35f;
    float slowTimer = 0.0f;

    // configure text sizes (mixed => D)
    int titleSize = 48;
    int buttonSize = 28;
    int scoreSize = 26;
    int uiSize = 22;

    // main loop
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        Vector2 mouse = GetMousePosition();

        // handle Start/Stop clicks
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (state == MENU || state == STOPPED) {
                if (CheckCollisionPointRec(mouse, btnStart)) {
                    // start
                    state = PLAY;
                    balls = initBalls();
                    score[1] = score[2] = 0;
                    currentPlayer = 1;
                    waitingPlacement = false;
                    shotInProgress = false;
                    charging = false;
                    power = 0.0f;
                    gameOver = false;
                    winner = -1;
                    ignoreInputFramesAfterStart = 6; // small grace
                    slowTimer = 0.0f;
                }
            } else if (state == PLAY) {
                if (CheckCollisionPointRec(mouse, btnStop)) {
                    // stop => back to menu
                    state = MENU;
                    balls = initBalls();
                    score[1] = score[2] = 0;
                    currentPlayer = 1;
                    waitingPlacement = false;
                    shotInProgress = false;
                    charging = false;
                    power = 0.0f;
                    gameOver = false;
                    winner = -1;
                    slowTimer = 0.0f;
                }
            }
        }

        // update gameplay only in PLAY
        if (state == PLAY && !gameOver) {
            if (ignoreInputFramesAfterStart > 0) ignoreInputFramesAfterStart--;

            Vector2 cuePos = balls[0].pos;
            float aimAngle = atan2f(mouse.y - cuePos.y, mouse.x - cuePos.x);

            // ball-in-hand placement
            if (waitingPlacement && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && ignoreInputFramesAfterStart == 0) {
                Vector2 p = mouse;
                p.x = clampf_custom(p.x, play.x + BALL_R, play.x + play.width - BALL_R);
                p.y = clampf_custom(p.y, play.y + BALL_R, play.y + play.height - BALL_R);
                bool ok = true;
                for (size_t i=1;i<balls.size();++i) if (balls[i].active && Dist(p, balls[i].pos) < 2.0f*BALL_R + 1.0f) ok = false;
                if (ok) { balls[0].pos = p; balls[0].vel = {0,0}; waitingPlacement = false; }
            }

            // shooting input
            if (ignoreInputFramesAfterStart == 0 && !shotInProgress && !waitingPlacement) {
                if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                    charging = true;
                    power += 0.45f;
                    if (power > MAX_POWER) power = MAX_POWER;
                }
                if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && charging) {
                    balls[0].vel.x = cosf(aimAngle) * (-power);
                    balls[0].vel.y = sinf(aimAngle) * (-power);
                    shotInProgress = true;
                    charging = false;
                    power = 0.0f;
                    slowTimer = 0.0f;
                }
            }

            // move balls
            for (auto &b : balls) {
                if (!b.active) continue;
                b.pos.x += b.vel.x;
                b.pos.y += b.vel.y;
                b.vel.x *= FRICTION;
                b.vel.y *= FRICTION;
                if (fabs(b.vel.x) < MIN_VEL) b.vel.x = 0.0f;
                if (fabs(b.vel.y) < MIN_VEL) b.vel.y = 0.0f;

                // near pocket
                bool nearPocket=false;
                for (auto &h: holes) if (Dist(b.pos,h) < HOLE_R + BALL_R + 8.0f) nearPocket=true;

                if (!nearPocket) {
                    if (b.pos.x < play.x) { b.pos.x = play.x; b.vel.x *= -1.0f; }
                    if (b.pos.x > play.x + play.width) { b.pos.x = play.x + play.width; b.vel.x *= -1.0f; }
                    if (b.pos.y < play.y) { b.pos.y = play.y; b.vel.y *= -1.0f; }
                    if (b.pos.y > play.y + play.height) { b.pos.y = play.y + play.height; b.vel.y *= -1.0f; }
                }
            }

            // ball-ball collisions
            for (size_t i=0;i<balls.size();++i)
                for (size_t j=i+1;j<balls.size();++j)
                    ResolveBallCollision(balls[i], balls[j], BALL_R);

            // cushion separation & reflect
            for (auto &seg : cushions) {
                for (auto &b : balls) {
                    if (!b.active) continue;
                    float t; Vector2 cp = ClosestPointOnSegment(seg.a, seg.b, b.pos, t);
                    float d = Dist(cp, b.pos);
                    if (d < BALL_R) {
                        Vector2 n = { b.pos.x - cp.x, b.pos.y - cp.y };
                        float nlen = sqrtf(n.x*n.x + n.y*n.y);
                        if (nlen < 1e-6f) continue;
                        Vector2 n_norm = { n.x/nlen, n.y/nlen };
                        float overlap = BALL_R - d;
                        b.pos.x += n_norm.x * overlap;
                        b.pos.y += n_norm.y * overlap;
                        float vdot = b.vel.x*n_norm.x + b.vel.y*n_norm.y;
                        b.vel.x -= 2.0f * vdot * n_norm.x;
                        b.vel.y -= 2.0f * vdot * n_norm.y;
                        b.vel.x *= 0.98f; b.vel.y *= 0.98f;
                    }
                }
            }

            // pockets detection
            std::vector<int> pocketed;
            for (auto &b : balls) {
                if (!b.active) continue;
                for (auto &h: holes) {
                    if (Dist(b.pos,h) < HOLE_R - 4.0f) {
                        pocketed.push_back(b.id);
                        if (b.id != 0) b.active = false;
                        // cue ball handled later (ball-in-hand)
                    }
                }
            }

            bool foul=false, scoredBall=false, pocket8=false;
            for (int id : pocketed) {
                if (id==0) foul=true;
                else if (id==8) pocket8=true;
                else { score[currentPlayer]++; scoredBall=true; }
            }
            if (foul) {
                score[currentPlayer] = std::max(0, score[currentPlayer]-1);
                balls[0].pos = { play.x + play.width*0.18f, play.y + play.height*0.5f };
                balls[0].vel = {0,0};
                currentPlayer = (currentPlayer==1?2:1);
                waitingPlacement = true;
                shotInProgress = false;
            }
            if (pocket8) { winner = currentPlayer; gameOver = true; state = STOPPED; }

            // early turn end detection
            bool allVerySlow=true;
            for (auto &b: balls) {
                if (!b.active) continue;
                float speed = sqrtf(b.vel.x*b.vel.x + b.vel.y*b.vel.y);
                if (speed > SLOW_THRESHOLD) { allVerySlow=false; break; }
            }
            if (allVerySlow && shotInProgress) slowTimer += dt; else slowTimer = 0.0f;
            if (slowTimer >= SLOW_DURATION && shotInProgress) {
                if (!foul && !scoredBall) currentPlayer = (currentPlayer==1?2:1);
                shotInProgress = false;
                slowTimer = 0.0f;
            }

        } // end PLAY update

        // ---------------- DRAW ----------------
        BeginDrawing();
        ClearBackground(DARKGREEN);

        // draw rails (wood)
        DrawRectangle((int)(TABLE.x - 35), (int)(TABLE.y - 35), (int)(TABLE.width + 70), 35, (Color){80,40,10,255});
        DrawRectangle((int)(TABLE.x - 35), (int)(TABLE.y + TABLE.height), (int)(TABLE.width + 70), 35, (Color){80,40,10,255});
        DrawRectangle((int)(TABLE.x - 35), (int)(TABLE.y - 35), 35, (int)(TABLE.height + 70), (Color){80,40,10,255});
        DrawRectangle((int)(TABLE.x + TABLE.width), (int)(TABLE.y - 35), 35, (int)(TABLE.height + 70), (Color){80,40,10,255});

        // draw play cloth and subtle texture stripes
        Color cloth = {10,120,60,255};
        DrawRectangleRec(play, cloth);
        for (int y=(int)play.y; y < (int)(play.y + play.height); y += 6) DrawLine((int)play.x, y, (int)(play.x + play.width), y, (Color){0,60,30,18});

        // draw pockets (funnel mouth) - black circles then a darker inner fade
        for (auto &h : holes) {
            DrawCircleV(h, HOLE_R, BLACK);
            DrawCircleV(h, HOLE_R*0.7f, (Color){0,0,0,200});
        }

        // draw cushions (no green pocket lines)
        for (auto &s : cushions) DrawLineEx(s.a, s.b, 6.0f * SCALE, (Color){18,80,20,200});

        // draw balls (textures centered if loaded)
        bool texOK = (ballTex[0].id != 0 && cueTex.id != 0 && customFont.texture.id != 0);
        for (auto &b : balls) {
            if (!b.active) continue;
            if (texOK && ballTex[b.id].id != 0) {
                Texture2D &tx = ballTex[b.id];
                Rectangle src = { 0, 0, (float)tx.width, (float)tx.height };
                Rectangle dst = { b.pos.x - BALL_R, b.pos.y - BALL_R, BALL_R*2.0f, BALL_R*2.0f };
                Vector2 origin = { BALL_R, BALL_R };
                DrawTexturePro(tx, src, dst, origin, 0.0f, WHITE);
            } else {
                // fallback
                DrawCircleV(b.pos, BALL_R, b.color);
                DrawCircleV({ b.pos.x - BALL_R*0.35f, b.pos.y - BALL_R*0.35f }, BALL_R*0.34f, (Color){255,255,255,80});
                DrawCircleV(b.pos, BALL_R*0.56f, WHITE);
                DrawText(TextFormat("%d", b.id), (int)(b.pos.x - BALL_R*0.35f), (int)(b.pos.y - BALL_R*0.55f), (int)BALL_R, BLACK);
                if (b.id >= 9 && b.id <= 15) DrawRectangle((int)(b.pos.x - BALL_R), (int)(b.pos.y - BALL_R*0.45f), (int)(BALL_R*2.0f), (int)(BALL_R*0.9f), WHITE);
            }
        }

        // draw cue and trajectory only in PLAY and when not shot and not waitingPlacement and not ignoring start frames
        if (state == PLAY && !shotInProgress && !waitingPlacement && ignoreInputFramesAfterStart == 0 && !gameOver) {
            Vector2 cuePos = balls[0].pos;
            Vector2 mousePos = GetMousePosition();
            float angle = atan2f(mousePos.y - cuePos.y, mousePos.x - cuePos.x);

            // draw cue: user's texture has tip on RIGHT
            if (cueTex.id != 0) {
                Texture2D &tx = cueTex;
                Rectangle src = { (float)tx.width, 0.0f, -(float)tx.width, (float)tx.height };
                float desiredLen = 180.0f * SCALE;
                float scaleX = desiredLen / (float)tx.width;
                float scaleY = (desiredLen / (float)tx.width);
                Rectangle dst = { cuePos.x - desiredLen*0.08f, cuePos.y - (float)tx.height*scaleY/2.0f, desiredLen, (float)tx.height*scaleY };
                Vector2 origin = { desiredLen*0.08f, (float)tx.height*scaleY/2.0f };
                DrawTexturePro(tx, src, dst, origin, angle*RAD2DEG, WHITE);
            } else {
                // fallback simple line
                Vector2 butt = cuePos;
                Vector2 tip = { cuePos.x + cosf(angle)*(180.0f*SCALE), cuePos.y + sinf(angle)*(180.0f*SCALE) };
                DrawLineEx(butt, tip, 10.0f*SCALE, (Color){181,101,29,255});
            }

            // TRAJECTORY: starts BEHIND cue ball
            Vector2 dirBack = { -cosf(angle), -sinf(angle) };
            Vector2 startTrace = { cuePos.x + dirBack.x * (BALL_R + 2.0f), cuePos.y + dirBack.y * (BALL_R + 2.0f) };
            const float MAX_TRACE = 1200.0f;

            // check pocket first
            float bestDist = 1e9f;
            Vector2 bestHit = { startTrace.x + dirBack.x * MAX_TRACE, startTrace.y + dirBack.y * MAX_TRACE };
            enum { NONE=0, HIT_BALL=1, HIT_CUSHION=2, HIT_POCKET=3 } hitType = NONE;
            Segment hitSeg;

            // check pockets
            for (auto &h : holes) {
                // project the vector
                Vector2 toHole = { h.x - startTrace.x, h.y - startTrace.y };
                float proj = toHole.x*dirBack.x + toHole.y*dirBack.y;
                if (proj < 0 || proj > MAX_TRACE) continue;
                // perpendicular distance
                Vector2 closest = { startTrace.x + dirBack.x * proj, startTrace.y + dirBack.y * proj };
                float perp = Dist(closest, h);
                if (perp <= HOLE_R) {
                    if (proj < bestDist) { bestDist = proj; bestHit = closest; hitType = HIT_POCKET; }
                }
            }

            // check balls
            for (auto &b : balls) {
                if (!b.active) continue;
                if (b.id == 0) continue;
                Vector2 p; float d;
                if (RayBallHit(startTrace, dirBack, b.pos, MAX_TRACE, BALL_R, p, d)) {
                    if (d < bestDist) { bestDist = d; bestHit = p; hitType = HIT_BALL; }
                }
            }

            // check cushions
            for (auto &s : cushions) {
                Vector2 p; float d;
                if (RaySegmentHit(startTrace, dirBack, s, MAX_TRACE, BALL_R, p, d)) {
                    if (d < bestDist) { bestDist = d; bestHit = p; hitType = HIT_CUSHION; hitSeg = s; }
                }
            }

            if (hitType == HIT_POCKET) {
                // draw dashed line to pocket but do not display any cushion bounce leg
                DrawDashedLine(startTrace, bestHit, 8.0f, 6.0f, WHITE);
            } else if (hitType == HIT_BALL) {
                DrawDashedLine(startTrace, bestHit, 8.0f, 6.0f, WHITE);
            } else if (hitType == HIT_CUSHION) {
                DrawDashedLine(startTrace, bestHit, 8.0f, 6.0f, WHITE);
                // reflect once
                Vector2 segDir = { hitSeg.b.x - hitSeg.a.x, hitSeg.b.y - hitSeg.a.y };
                float segLen = sqrtf(segDir.x*segDir.x + segDir.y*segDir.y);
                if (segLen > 1e-6f) {
                    // normal pointing into play
                    Vector2 segNorm = { -segDir.y/segLen, segDir.x/segLen };
                    // ensure normal points against incoming direction
                    if (segNorm.x * dirBack.x + segNorm.y * dirBack.y > 0) { segNorm.x *= -1.0f; segNorm.y *= -1.0f; }
                    Vector2 refl = Reflect(dirBack, segNorm);
                    Vector2 secondStart = { bestHit.x + refl.x * (BALL_R * 0.6f), bestHit.y + refl.y * (BALL_R * 0.6f) };
                    // second leg stops on ball or pocket
                    float bestDist2 = 1e9f; Vector2 bestHit2 = { secondStart.x + refl.x * 600.0f, secondStart.y + refl.y * 600.0f };
                    int hitType2 = NONE;
                    // pockets check
                    for (auto &h : holes) {
                        Vector2 toHole = { h.x - secondStart.x, h.y - secondStart.y };
                        float proj = toHole.x*refl.x + toHole.y*refl.y;
                        if (proj < 0 || proj > 600.0f) continue;
                        Vector2 closest = { secondStart.x + refl.x * proj, secondStart.y + refl.y * proj };
                        float perp = Dist(closest, h);
                        if (perp <= HOLE_R) {
                            if (proj < bestDist2) { bestDist2 = proj; bestHit2 = closest; hitType2 = HIT_POCKET; }
                        }
                    }
                    // balls check
                    for (auto &b : balls) {
                        if (!b.active) continue;
                        if (b.id == 0) continue;
                        Vector2 p; float d;
                        if (RayBallHit(secondStart, refl, b.pos, 600.0f, BALL_R, p, d)) {
                            if (d < bestDist2) { bestDist2 = d; bestHit2 = p; hitType2 = HIT_BALL; }
                        }
                    }
                    // cushions for second leg NOT allowed to bounce again (max 1)
                    DrawDashedLine(secondStart, bestHit2, 8.0f, 6.0f, WHITE);
                }
            } else {
                // nothing hit
                Vector2 full = { startTrace.x + dirBack.x * MAX_TRACE, startTrace.y + dirBack.y * MAX_TRACE };
                DrawDashedLine(startTrace, full, 8.0f, 6.0f, WHITE);
            }
        } // end draw cue+trajectory

        // draw UI
        if (customFont.texture.id != 0) {
            DrawTextEx(customFont, "Power:", {20, 18}, uiSize, 0.0f, WHITE);
            DrawRectangle(110, 20, 300, 18, LIGHTGRAY);
            DrawRectangle(110, 20, (int)((power/MAX_POWER)*300.0f), 18, ORANGE);

            DrawTextEx(customFont, TextFormat("Turn: Player %d", currentPlayer), { SCREEN_W*0.5f - 70, 18 }, uiSize+2, 0.0f, YELLOW);
            DrawTextEx(customFont, TextFormat("P1: %d", score[1]), {20, SCREEN_H - 88}, scoreSize, 0.0f, WHITE);
            DrawTextEx(customFont, TextFormat("P2: %d", score[2]), {20, SCREEN_H - 52}, scoreSize, 0.0f, WHITE);
            if (state == MENU || state == STOPPED) {
                DrawRectangleRec(btnStart, (Color){40,40,40,220});
                DrawRectangleLinesEx(btnStart, 2, Fade(RAYWHITE, 0.06f));
                DrawTextEx(customFont, "START", { btnStart.x + btnStart.width*0.14f, btnStart.y + (btnStart.height - titleSize)/2.0f }, titleSize, 0.0f, RAYWHITE);
            } else {
                DrawRectangleRec(btnStop, (Color){160,40,40,220});
                DrawTextEx(customFont, "STOP", { btnStop.x + 18, btnStop.y + 6 }, buttonSize, 0.0f, RAYWHITE);
            }
            if (state == STOPPED) {
                const char *res = (winner>0)?TextFormat("WINNER: Player %d", winner):"DRAW";
                DrawTextEx(customFont, res, { SCREEN_W*0.5f - 140, SCREEN_H*0.5f - 120 }, 34, 0.0f, GOLD);
            }
        } else {
            // fallback UI using default font
            DrawText("Power:", 20, 18, uiSize, WHITE);
            DrawRectangle(110, 20, 300, 18, LIGHTGRAY);
            DrawRectangle(110, 20, (int)((power/MAX_POWER)*300.0f), 18, ORANGE);
            DrawText(TextFormat("Turn: Player %d", currentPlayer), SCREEN_W/2 - 70, 18, uiSize+2, YELLOW);
            DrawText(TextFormat("P1: %d", score[1]), 20, SCREEN_H - 88, scoreSize, WHITE);
            DrawText(TextFormat("P2: %d", score[2]), 20, SCREEN_H - 52, scoreSize, WHITE);
            if (state == MENU || state == STOPPED) {
                DrawRectangleRec(btnStart, (Color){40,40,40,220});
                DrawText("START", (int)(btnStart.x + btnStart.width*0.28f), (int)(btnStart.y + btnStart.height*0.28f), titleSize, RAYWHITE);
            } else {
                DrawRectangleRec(btnStop, (Color){160,40,40,220});
                DrawText("STOP", (int)(btnStop.x + 18), (int)(btnStop.y + 6), buttonSize, RAYWHITE);
            }
            if (state == STOPPED) {
                if (winner>0) DrawText(TextFormat("WINNER: Player %d", winner), SCREEN_W/2 - 140, SCREEN_H/2 - 120, 34, GOLD);
                else DrawText("DRAW", SCREEN_W/2 - 40, SCREEN_H/2 - 120, 34, GOLD);
            }
        }

        EndDrawing();

        // restart quick R
        if (IsKeyPressed(KEY_R)) {
            state = PLAY;
            balls = initBalls();
            score[1]=score[2]=0;
            currentPlayer = 1;
            waitingPlacement = false;
            shotInProgress = false;
            charging = false;
            power = 0.0f;
            gameOver = false;
            winner = -1;
            slowTimer = 0.0f;
        }

    } // main loop

    // cleanup
    for (int i=0;i<16;i++) if (ballTex[i].id != 0) UnloadTexture(ballTex[i]);
    if (cueTex.id != 0) UnloadTexture(cueTex);
    if (customFont.texture.id != 0) UnloadFont(customFont);

    CloseWindow();
    return 0;
}
