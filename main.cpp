// ============================================================
// UPGRADED 2D CAR RACING GAME - GLUT VERSION
// By MD RAKIB | Enhanced Version
// Features:
//   - All 3 lanes always spawn enemies
//   - Lives system (3 lives)
//   - Shield & Speed powerups
//   - Particle explosion effects
//   - Screen shake on collision
//   - Scrolling background trees
//   - Invincibility frames after hit
//   - Dynamic speed & level scaling
// ============================================================

#include <GL/glut.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <vector>
#include <algorithm>
#include <ctime>

using namespace std;

// ============================================================
// CONSTANTS
// ============================================================
const int WIN_W = 500;
const int WIN_H = 700;
const int FPS   = 60;

const float ROAD_LEFT  = -0.65f;
const float ROAD_RIGHT =  0.65f;
const float ROAD_W     = ROAD_RIGHT - ROAD_LEFT;
const float LANE_W     = ROAD_W / 3.0f;

// Lane center X positions
const float LANE_CX[3] = {
    ROAD_LEFT + LANE_W * 0.5f - 0.04f,
    ROAD_LEFT + LANE_W * 1.5f - 0.04f,
    ROAD_LEFT + LANE_W * 2.5f - 0.04f
};

const float CAR_W = 0.10f;
const float CAR_H = 0.16f;

// ============================================================
// GAME STATE
// ============================================================
enum GameState { MENU, PLAYING, GAMEOVER };
GameState gameState = MENU;

int   score       = 0;
int   level       = 1;
int   lives       = 3;
float baseSpeed   = 0.018f;
int   frameCount  = 0;

// ============================================================
// PLAYER
// ============================================================
struct Player {
    float x, y;
    float targetX;
    int   lane;
    int   invincible;   // frames of invincibility
    bool  shielded;
    int   shieldTimer;
    bool  boosting;
    int   boostTimer;
} player;

void initPlayer() {
    player.lane       = 1;
    player.x          = LANE_CX[1];
    player.targetX    = LANE_CX[1];
    player.y          = -0.75f;
    player.invincible = 0;
    player.shielded   = false;
    player.shieldTimer= 0;
    player.boosting   = false;
    player.boostTimer = 0;
}

float getSpeed() {
    float spd = baseSpeed + (level - 1) * 0.003f;
    if (player.boosting) spd += 0.012f;
    return spd;
}

// ============================================================
// ENEMY
// ============================================================
struct EnemyCar {
    float x, y;
    float speed;
    int   lane;
    int   colorType;  // 0=red 1=orange 2=purple 3=green 4=pink
};

vector<EnemyCar> enemies;
int spawnCooldown[3] = {0, 0, 0};
const int SPAWN_BASE = 120;

void initEnemy(EnemyCar &e, int lane) {
    e.lane      = lane;
    e.x         = LANE_CX[lane];
    e.y         = 1.15f + (rand() % 40) / 100.0f;
    e.speed     = getSpeed() * (0.75f + (rand() % 40) / 100.0f);
    e.colorType = rand() % 5;
}

void spawnEnemies() {
    for (int l = 0; l < 3; l++) {
        if (spawnCooldown[l] > 0) { spawnCooldown[l]--; continue; }

        // Check if lane is clear near top
        bool busy = false;
        for (auto &e : enemies)
            if (e.lane == l && e.y > 0.5f) { busy = true; break; }

        float spawnChance = 0.02f + level * 0.004f;
        if (!busy && (float)(rand() % 1000) / 1000.0f < spawnChance) {
            EnemyCar ne;
            initEnemy(ne, l);
            enemies.push_back(ne);
            int cd = SPAWN_BASE - level * 8;
            if (cd < 25) cd = 25;
            spawnCooldown[l] = cd + rand() % 30;
        }
    }
}

// ============================================================
// POWERUP
// ============================================================
struct Powerup {
    float x, y;
    int   lane;
    int   type;  // 0=shield 1=boost
    float pulse;
};

vector<Powerup> powerups;

void spawnPowerup() {
    if ((int)powerups.size() >= 2) return;
    if ((rand() % 1000) < 3) {
        Powerup p;
        p.lane  = rand() % 3;
        p.x     = LANE_CX[p.lane] + CAR_W * 0.5f;
        p.y     = 1.1f;
        p.type  = rand() % 2;
        p.pulse = 0.0f;
        powerups.push_back(p);
    }
}

// ============================================================
// PARTICLE
// ============================================================
struct Particle {
    float x, y, vx, vy;
    float r, g, b;
    float life;
    float size;
};

vector<Particle> particles;

void addParticles(float x, float y, float pr, float pg, float pb, int n = 14) {
    for (int i = 0; i < n; i++) {
        Particle p;
        float angle = (float)(rand() % 628) / 100.0f;
        float speed = 0.01f + (rand() % 30) / 1000.0f;
        p.x    = x;  p.y    = y;
        p.vx   = cosf(angle) * speed;
        p.vy   = sinf(angle) * speed;
        p.r    = pr; p.g    = pg; p.b    = pb;
        p.life = 1.0f;
        p.size = 0.008f + (rand() % 8) / 1000.0f;
        particles.push_back(p);
    }
}

// ============================================================
// SCREEN SHAKE
// ============================================================
float shakeX = 0, shakeY = 0;
int   shakeTimer = 0;

void triggerShake(int dur = 20) {
    shakeTimer = dur;
}

void updateShake() {
    if (shakeTimer > 0) {
        shakeX = ((rand() % 200) - 100) / 5000.0f;
        shakeY = ((rand() % 200) - 100) / 5000.0f;
        shakeTimer--;
    } else {
        shakeX = shakeY = 0.0f;
    }
}

// ============================================================
// BACKGROUND
// ============================================================
float dashY      = 0.0f;
float starField[80][3]; // x, y, brightness

struct Tree {
    float x, y, h, scrollSpeed;
};
Tree trees[12];

void initBackground() {
    for (int i = 0; i < 80; i++) {
        starField[i][0] = -1.0f + (rand() % 200) / 100.0f;
        starField[i][1] = -1.0f + (rand() % 200) / 100.0f;
        starField[i][2] = 0.4f  + (rand() % 60)  / 100.0f;
    }
    for (int i = 0; i < 6; i++) {
        trees[i].x = -0.95f + (rand() % 30) / 100.0f;
        trees[i].y = -1.0f  + (rand() % 200) / 100.0f;
        trees[i].h = 0.08f  + (rand() % 8)   / 100.0f;
        trees[i].scrollSpeed = 0.4f + (rand() % 30) / 100.0f;
    }
    for (int i = 6; i < 12; i++) {
        trees[i].x = 0.70f + (rand() % 30) / 100.0f;
        trees[i].y = -1.0f + (rand() % 200) / 100.0f;
        trees[i].h = 0.08f + (rand() % 8)   / 100.0f;
        trees[i].scrollSpeed = 0.4f + (rand() % 30) / 100.0f;
    }
}

// ============================================================
// DRAW HELPERS
// ============================================================
void drawRect(float x, float y, float w, float h, float r, float g, float b, float a = 1.0f) {
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
    glVertex2f(x,     y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x,     y + h);
    glEnd();
}

void drawText(float x, float y, const char *text, float r, float g, float b, void *font = GLUT_BITMAP_HELVETICA_18) {
    glColor3f(r, g, b);
    glRasterPos2f(x, y);
    for (int i = 0; text[i] != '\0'; i++)
        glutBitmapCharacter(font, text[i]);
}

void drawCircle(float cx, float cy, float rad, float r, float g, float b, float a = 1.0f, int segs = 24) {
    glColor4f(r, g, b, a);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (int i = 0; i <= segs; i++) {
        float angle = (float)i / segs * 2.0f * 3.14159f;
        glVertex2f(cx + cosf(angle) * rad, cy + sinf(angle) * rad);
    }
    glEnd();
}

// ============================================================
// DRAW CAR
// ============================================================
void getEnemyColor(int type, float &r, float &g, float &b) {
    switch (type) {
        case 0: r=1.0f; g=0.2f; b=0.2f; break; // red
        case 1: r=1.0f; g=0.6f; b=0.0f; break; // orange
        case 2: r=0.8f; g=0.2f; b=1.0f; break; // purple
        case 3: r=0.2f; g=1.0f; b=0.5f; break; // green
        default:r=1.0f; g=0.3f; b=0.7f; break; // pink
    }
}

void drawCar(float x, float y, float r, float g, float b, float alpha = 1.0f) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    float W = CAR_W, H = CAR_H;

    // Body
    glColor4f(r, g, b, alpha);
    glBegin(GL_QUADS);
    glVertex2f(x,     y + 0.02f);
    glVertex2f(x + W, y + 0.02f);
    glVertex2f(x + W, y + H - 0.02f);
    glVertex2f(x,     y + H - 0.02f);
    glEnd();

    // Cabin
    glColor4f(r * 0.6f, g * 0.6f, b * 0.6f, alpha);
    glBegin(GL_QUADS);
    glVertex2f(x + 0.015f, y + 0.05f);
    glVertex2f(x + W - 0.015f, y + 0.05f);
    glVertex2f(x + W - 0.015f, y + H * 0.68f);
    glVertex2f(x + 0.015f, y + H * 0.68f);
    glEnd();

    // Windshield
    glColor4f(0.5f, 0.85f, 1.0f, alpha * 0.7f);
    glBegin(GL_QUADS);
    glVertex2f(x + 0.018f, y + 0.055f);
    glVertex2f(x + W - 0.018f, y + 0.055f);
    glVertex2f(x + W - 0.018f, y + H * 0.55f);
    glVertex2f(x + 0.018f, y + H * 0.55f);
    glEnd();

    // Wheels (4)
    float wy1 = y + 0.022f, wy2 = y + H * 0.65f;
    float wOffsets[2] = { x - 0.012f, x + W - 0.012f };
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            drawRect(wOffsets[i], (j == 0 ? wy1 : wy2), 0.018f, 0.03f, 0.05f, 0.05f, 0.05f, alpha);
        }
    }

    // Headlights
    glColor4f(1.0f, 1.0f, 0.6f, alpha);
    glBegin(GL_QUADS);
    glVertex2f(x + 0.005f,     y + H - 0.025f);
    glVertex2f(x + 0.03f,      y + H - 0.025f);
    glVertex2f(x + 0.03f,      y + H - 0.01f);
    glVertex2f(x + 0.005f,     y + H - 0.01f);
    glEnd();
    glBegin(GL_QUADS);
    glVertex2f(x + W - 0.03f,  y + H - 0.025f);
    glVertex2f(x + W - 0.005f, y + H - 0.025f);
    glVertex2f(x + W - 0.005f, y + H - 0.01f);
    glVertex2f(x + W - 0.03f,  y + H - 0.01f);
    glEnd();

    // Taillights
    glColor4f(1.0f, 0.1f, 0.1f, alpha);
    glBegin(GL_QUADS);
    glVertex2f(x + 0.005f,     y + 0.01f);
    glVertex2f(x + 0.025f,     y + 0.01f);
    glVertex2f(x + 0.025f,     y + 0.025f);
    glVertex2f(x + 0.005f,     y + 0.025f);
    glEnd();
    glBegin(GL_QUADS);
    glVertex2f(x + W - 0.025f, y + 0.01f);
    glVertex2f(x + W - 0.005f, y + 0.01f);
    glVertex2f(x + W - 0.005f, y + 0.025f);
    glVertex2f(x + W - 0.025f, y + 0.025f);
    glEnd();

    glDisable(GL_BLEND);
}

void drawPlayerCar() {
    // Boost flame
    if (player.boosting) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        for (int i = 0; i < 3; i++) {
            float sz = 0.02f + i * 0.01f;
            float ya = player.y - 0.015f - i * 0.025f;
            glColor4f(0.0f, 0.5f + i * 0.2f, 1.0f, 0.6f - i * 0.15f);
            glBegin(GL_TRIANGLES);
            glVertex2f(player.x + CAR_W * 0.3f, ya);
            glVertex2f(player.x + CAR_W * 0.7f, ya);
            glVertex2f(player.x + CAR_W * 0.5f, ya - sz);
            glEnd();
        }
        glDisable(GL_BLEND);
    }

    bool blink = (player.invincible > 0 && (player.invincible / 4) % 2 == 0);
    if (!blink) {
        drawCar(player.x, player.y, 0.0f, 0.9f, 1.0f);
    }

    // Shield ring
    if (player.shielded) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        float pulse = 0.04f + sinf(frameCount * 0.15f) * 0.01f;
        glColor4f(0.2f, 0.6f, 1.0f, 0.35f);
        glLineWidth(2.5f);
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < 36; i++) {
            float ang = i / 36.0f * 2.0f * 3.14159f;
            glVertex2f(player.x + CAR_W * 0.5f + cosf(ang) * (0.1f + pulse),
                       player.y + CAR_H * 0.5f + sinf(ang) * (0.14f + pulse));
        }
        glEnd();
        glDisable(GL_BLEND);
    }
}

// ============================================================
// DRAW BACKGROUND
// ============================================================
void drawBackground() {
    // Sky gradient (simulated)
    drawRect(-1, -1, 2, 2, 0.05f, 0.05f, 0.18f);

    // Stars
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glPointSize(2.5f);
    glBegin(GL_POINTS);
    for (int i = 0; i < 80; i++) {
        float b = starField[i][2];
        float twinkle = b + sinf(frameCount * 0.05f + i) * 0.15f;
        glColor4f(twinkle, twinkle, twinkle * 0.9f, 1.0f);
        glVertex2f(starField[i][0], starField[i][1]);
    }
    glEnd();
    glDisable(GL_BLEND);

    // Grass
    drawRect(-1.0f, -1.0f, 0.35f, 2.0f, 0.05f, 0.28f, 0.05f);
    drawRect(0.65f, -1.0f, 0.35f, 2.0f, 0.05f, 0.28f, 0.05f);

    // Trees
    for (int i = 0; i < 12; i++) {
        float tx = trees[i].x;
        float ty = trees[i].y;
        float th = trees[i].h;
        // Trunk
        drawRect(tx + 0.01f, ty, 0.015f, th * 0.4f, 0.35f, 0.22f, 0.1f);
        // Leaves (triangle)
        glColor3f(0.1f, 0.45f, 0.1f);
        glBegin(GL_TRIANGLES);
        glVertex2f(tx + 0.018f, ty + th * 1.0f);
        glVertex2f(tx + 0.055f, ty + th * 1.0f);
        glVertex2f(tx + 0.036f, ty + th * 0.38f);
        glEnd();
    }

    // Road surface
    drawRect(ROAD_LEFT, -1.0f, ROAD_W, 2.0f, 0.15f, 0.15f, 0.15f);

    // Road borders
    drawRect(ROAD_LEFT - 0.02f, -1.0f, 0.02f, 2.0f, 0.9f, 0.9f, 0.9f);
    drawRect(ROAD_RIGHT, -1.0f, 0.02f, 2.0f, 0.9f, 0.9f, 0.9f);

    // Lane dividers (dashed)
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    for (int lane = 1; lane < 3; lane++) {
        float lx = ROAD_LEFT + lane * LANE_W;
        for (float y = -1.0f; y < 1.2f; y += 0.22f) {
            float dy = y + dashY;
            if (dy > 1.2f) dy -= 2.2f;
            drawRect(lx - 0.012f, dy, 0.024f, 0.12f, 1.0f, 1.0f, 0.0f, 0.7f);
        }
    }
    glDisable(GL_BLEND);
}

// ============================================================
// DRAW POWERUPS
// ============================================================
void drawPowerups() {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    for (auto &p : powerups) {
        float pulse = sinf(frameCount * 0.1f) * 0.015f;
        float cx = p.x - 0.01f;
        float cy = p.y + 0.04f;
        float rad = 0.04f + pulse;

        if (p.type == 0) { // Shield - blue
            drawCircle(cx, cy, rad + 0.01f, 0.2f, 0.5f, 1.0f, 0.3f);
            drawCircle(cx, cy, rad, 0.3f, 0.6f, 1.0f, 0.9f);
            drawText(cx - 0.015f, cy - 0.012f, "S", 1.0f, 1.0f, 1.0f, GLUT_BITMAP_HELVETICA_18);
        } else { // Boost - yellow
            drawCircle(cx, cy, rad + 0.01f, 1.0f, 0.8f, 0.0f, 0.3f);
            drawCircle(cx, cy, rad, 1.0f, 0.9f, 0.0f, 0.9f);
            drawText(cx - 0.01f, cy - 0.012f, "B", 0.1f, 0.1f, 0.1f, GLUT_BITMAP_HELVETICA_18);
        }
    }
    glDisable(GL_BLEND);
}

// ============================================================
// DRAW PARTICLES
// ============================================================
void drawParticles() {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glPointSize(4.0f);
    for (auto &p : particles) {
        glColor4f(p.r, p.g, p.b, p.life);
        glBegin(GL_QUADS);
        glVertex2f(p.x - p.size, p.y - p.size);
        glVertex2f(p.x + p.size, p.y - p.size);
        glVertex2f(p.x + p.size, p.y + p.size);
        glVertex2f(p.x - p.size, p.y + p.size);
        glEnd();
    }
    glDisable(GL_BLEND);
}

// ============================================================
// DRAW HUD
// ============================================================
void drawHUD() {
    // Top bar
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    drawRect(-1.0f, 0.88f, 2.0f, 0.12f, 0.0f, 0.0f, 0.0f, 0.6f);
    glDisable(GL_BLEND);

    char buf[64];
    sprintf(buf, "SCORE: %d", score);
    drawText(-0.95f, 0.92f, buf, 1.0f, 0.9f, 0.0f);
    sprintf(buf, "LVL: %d", level);
    drawText(-0.25f, 0.92f, buf, 0.4f, 1.0f, 0.6f);
    sprintf(buf, "SPD: %d", (int)(getSpeed() * 1000));
    drawText(0.30f, 0.92f, buf, 0.4f, 0.8f, 1.0f);

    // Lives (hearts)
    for (int i = 0; i < lives; i++) {
        drawCircle(-0.97f + i * 0.12f, 0.80f, 0.025f, 1.0f, 0.1f, 0.2f);
        drawCircle(-0.95f + i * 0.12f, 0.80f, 0.025f, 1.0f, 0.1f, 0.2f);
        glColor3f(1.0f, 0.1f, 0.2f);
        glBegin(GL_TRIANGLES);
        glVertex2f(-0.99f + i * 0.12f, 0.787f);
        glVertex2f(-0.93f + i * 0.12f, 0.787f);
        glVertex2f(-0.96f + i * 0.12f, 0.763f);
        glEnd();
    }

    // Boost bar
    if (player.boosting && player.boostTimer > 0) {
        float ratio = player.boostTimer / 180.0f;
        drawRect(-0.95f, 0.70f, 1.0f, 0.025f, 0.2f, 0.2f, 0.2f);
        drawRect(-0.95f, 0.70f, ratio * 1.0f, 0.025f, 0.2f, 0.6f, 1.0f);
        drawText(-0.95f, 0.74f, "BOOST", 0.2f, 0.7f, 1.0f, GLUT_BITMAP_HELVETICA_12);
    }

    // Shield indicator
    if (player.shielded && player.shieldTimer > 0) {
        float ratio = player.shieldTimer / 360.0f;
        drawRect(-0.95f, 0.64f, 1.0f, 0.025f, 0.2f, 0.2f, 0.2f);
        drawRect(-0.95f, 0.64f, ratio * 1.0f, 0.025f, 0.2f, 0.4f, 1.0f);
        drawText(-0.95f, 0.68f, "SHIELD", 0.3f, 0.5f, 1.0f, GLUT_BITMAP_HELVETICA_12);
    }
}

// ============================================================
// DRAW MENU
// ============================================================
void drawMenu() {
    drawBackground();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    drawRect(-0.55f, -0.45f, 1.1f, 1.0f, 0.0f, 0.0f, 0.25f, 0.85f);
    glDisable(GL_BLEND);

    glColor3f(0.0f, 0.9f, 1.0f);
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(-0.55f, -0.45f); glVertex2f(0.55f, -0.45f);
    glVertex2f(0.55f,  0.55f);  glVertex2f(-0.55f, 0.55f);
    glEnd();

    drawText(-0.38f, 0.40f, "2D CAR RACING", 0.0f, 0.9f, 1.0f);
    drawText(-0.20f, 0.28f, "by MD RAKIB", 1.0f, 0.85f, 0.0f, GLUT_BITMAP_HELVETICA_12);

    if (gameState == GAMEOVER) {
        drawText(-0.25f, 0.15f, "GAME OVER!", 1.0f, 0.1f, 0.1f);
        char s[64];
        sprintf(s, "Score: %d   Level: %d", score, level);
        drawText(-0.30f, 0.04f, s, 1.0f, 0.5f, 0.5f, GLUT_BITMAP_HELVETICA_12);
    }

    drawText(-0.35f, -0.08f, "SPACE  = Start / Restart", 0.2f, 1.0f, 0.4f, GLUT_BITMAP_HELVETICA_12);
    drawText(-0.35f, -0.17f, "LEFT/RIGHT or A/D = Move", 1.0f, 1.0f, 1.0f, GLUT_BITMAP_HELVETICA_12);
    drawText(-0.35f, -0.26f, "UP Arrow = Speed Boost", 1.0f, 1.0f, 0.0f, GLUT_BITMAP_HELVETICA_12);
    drawText(-0.35f, -0.35f, "ESC = Exit", 0.8f, 0.5f, 0.5f, GLUT_BITMAP_HELVETICA_12);

    // Powerup legend
    drawText(-0.35f, -0.56f, "[S] = Shield  [B] = Speed Boost", 0.4f, 0.7f, 1.0f, GLUT_BITMAP_HELVETICA_12);
    drawText(-0.35f, -0.65f, "3 Lives  |  All Lanes Active", 0.8f, 0.8f, 0.8f, GLUT_BITMAP_HELVETICA_12);
}

// ============================================================
// COLLISION DETECTION
// ============================================================
bool rectCollide(float x1, float y1, float w1, float h1,
                 float x2, float y2, float w2, float h2) {
    return x1 < x2 + w2 && x1 + w1 > x2 &&
           y1 < y2 + h2 && y1 + h1 > y2;
}

void checkCollisions() {
    // Powerup pickup
    for (int i = (int)powerups.size() - 1; i >= 0; i--) {
        Powerup &p = powerups[i];
        if (rectCollide(player.x, player.y, CAR_W, CAR_H,
                        p.x - CAR_W * 0.5f, p.y, CAR_W, CAR_H)) {
            if (p.type == 0) {
                player.shielded   = true;
                player.shieldTimer= 360;
                addParticles(player.x + CAR_W * 0.5f, player.y + CAR_H * 0.5f, 0.3f, 0.6f, 1.0f);
            } else {
                player.boosting  = true;
                player.boostTimer= 180;
                addParticles(player.x + CAR_W * 0.5f, player.y + CAR_H * 0.5f, 1.0f, 0.8f, 0.1f);
            }
            powerups.erase(powerups.begin() + i);
        }
    }

    if (player.invincible > 0) return;

    for (int i = (int)enemies.size() - 1; i >= 0; i--) {
        EnemyCar &e = enemies[i];
        if (rectCollide(player.x + 0.01f, player.y + 0.01f,
                        CAR_W - 0.02f,    CAR_H - 0.02f,
                        e.x + 0.01f,      e.y + 0.01f,
                        CAR_W - 0.02f,    CAR_H - 0.02f)) {

            float er, eg, eb;
            getEnemyColor(e.colorType, er, eg, eb);
            addParticles(player.x + CAR_W * 0.5f, player.y + CAR_H * 0.4f, er, eg, eb, 20);
            addParticles(player.x + CAR_W * 0.5f, player.y + CAR_H * 0.4f, 1.0f, 0.5f, 0.0f, 8);
            triggerShake(25);
            enemies.erase(enemies.begin() + i);

            if (player.shielded) {
                player.shielded    = false;
                player.shieldTimer = 0;
                player.invincible  = 60;
            } else {
                lives--;
                if (lives <= 0) {
                    gameState = GAMEOVER;
                } else {
                    player.invincible = 100;
                }
            }
            break;
        }
    }
}

// ============================================================
// DISPLAY
// ============================================================
void display() {
    glClear(GL_COLOR_BUFFER_BIT);
    glLoadIdentity();

    if (gameState == MENU || gameState == GAMEOVER) {
        drawMenu();
        glutSwapBuffers();
        return;
    }

    glTranslatef(shakeX, shakeY, 0.0f);

    drawBackground();

    // Enemies
    for (size_t i = 0; i < enemies.size(); i++) {
        float r, g, b;
        getEnemyColor(enemies[i].colorType, r, g, b);
        float alpha = 1.0f;
        if (enemies[i].y > 0.85f) alpha = 1.0f - (enemies[i].y - 0.85f) / 0.35f;
        if (alpha < 0.0f) alpha = 0.0f;
        drawCar(enemies[i].x, enemies[i].y, r, g, b, alpha);
    }

    drawPowerups();
    drawPlayerCar();
    drawParticles();
    drawHUD();

    glutSwapBuffers();
}

// ============================================================
// UPDATE
// ============================================================
void update(int value) {
    if (gameState == PLAYING) {
        frameCount++;
        updateShake();

        // Scroll road
        dashY -= getSpeed() * 1.5f;
        if (dashY < -0.22f) dashY += 0.22f;

        // Scroll trees
        for (int i = 0; i < 12; i++) {
            trees[i].y -= getSpeed() * trees[i].scrollSpeed;
            if (trees[i].y < -1.1f) {
                trees[i].y = 1.1f;
                trees[i].x = (i < 6)
                    ? -0.95f + (rand() % 30) / 100.0f
                    :  0.70f + (rand() % 30) / 100.0f;
                trees[i].h = 0.08f + (rand() % 8) / 100.0f;
            }
        }

        // Smooth player movement
        player.x += (player.targetX - player.x) * 0.15f;

        // Invincibility countdown
        if (player.invincible > 0) player.invincible--;

        // Shield countdown
        if (player.shielded) {
            player.shieldTimer--;
            if (player.shieldTimer <= 0) { player.shielded = false; player.shieldTimer = 0; }
        }

        // Boost countdown
        if (player.boosting && player.boostTimer > 0) {
            player.boostTimer--;
            if (player.boostTimer <= 0) { player.boosting = false; }
        }

        // Spawn
        spawnEnemies();
        spawnPowerup();

        // Move enemies
        for (auto &e : enemies) e.y -= e.speed;

        // Score enemies that pass
        for (int i = (int)enemies.size() - 1; i >= 0; i--) {
            if (enemies[i].y < -1.2f) {
                enemies.erase(enemies.begin() + i);
                score++;
                if (score % 10 == 0) {
                    level++;
                    baseSpeed += 0.002f;
                }
            }
        }

        // Move powerups
        for (auto &p : powerups) p.y -= getSpeed() * 0.85f;
        powerups.erase(remove_if(powerups.begin(), powerups.end(),
            [](const Powerup &p){ return p.y < -1.2f; }), powerups.end());

        // Update particles
        for (auto &p : particles) {
            p.x    += p.vx;
            p.y    += p.vy;
            p.vy   -= 0.0005f;
            p.life -= 0.025f;
            p.size *= 0.98f;
        }
        particles.erase(remove_if(particles.begin(), particles.end(),
            [](const Particle &p){ return p.life <= 0.0f; }), particles.end());

        // Scroll stars
        for (int i = 0; i < 80; i++) {
            starField[i][1] -= 0.002f;
            if (starField[i][1] < -1.0f) {
                starField[i][1] = 1.0f;
                starField[i][0] = -1.0f + (rand() % 200) / 100.0f;
            }
        }

        checkCollisions();
    }

    glutPostRedisplay();
    glutTimerFunc(1000 / FPS, update, 0);
}

// ============================================================
// INPUT
// ============================================================
void startNewGame() {
    score      = 0;
    level      = 1;
    lives      = 3;
    baseSpeed  = 0.018f;
    frameCount = 0;
    enemies.clear();
    particles.clear();
    powerups.clear();
    // fix: use existing spawnCooldown array name
    spawnCooldown[0] = spawnCooldown[1] = spawnCooldown[2] = 0;

    // Pre-spawn one enemy per lane
    for (int l = 0; l < 3; l++) {
        EnemyCar e;
        initEnemy(e, l);
        e.y = 0.5f + l * 0.5f;
        enemies.push_back(e);
    }

    initPlayer();
    gameState = PLAYING;
}

void keyboard(unsigned char key, int x, int y) {
    switch (key) {
        case 27: exit(0); break;
        case ' ':
            if (gameState != PLAYING) startNewGame();
            break;
        case 'a': case 'A':
            if (gameState == PLAYING) {
                player.lane = max(0, player.lane - 1);
                player.targetX = LANE_CX[player.lane];
            }
            break;
        case 'd': case 'D':
            if (gameState == PLAYING) {
                player.lane = min(2, player.lane + 1);
                player.targetX = LANE_CX[player.lane];
            }
            break;
    }
}

void specialKeys(int key, int x, int y) {
    if (gameState != PLAYING) return;
    switch (key) {
        case GLUT_KEY_LEFT:
            player.lane = max(0, player.lane - 1);
            player.targetX = LANE_CX[player.lane];
            break;
        case GLUT_KEY_RIGHT:
            player.lane = min(2, player.lane + 1);
            player.targetX = LANE_CX[player.lane];
            break;
        case GLUT_KEY_UP:
            player.boosting   = true;
            player.boostTimer = max(player.boostTimer, 60);
            break;
    }
}

void specialKeysUp(int key, int x, int y) {
    if (key == GLUT_KEY_UP) {
        if (player.boostTimer <= 0) player.boosting = false;
    }
}

// ============================================================
// INIT & MAIN
// ============================================================
void initGL() {
    glClearColor(0.05f, 0.05f, 0.18f, 1.0f);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(-1.0, 1.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

int main(int argc, char **argv) {
    srand((unsigned)time(nullptr));
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
    glutInitWindowSize(WIN_W, WIN_H);
    glutCreateWindow("2D Car Racing - MD RAKIB");
    initGL();
    initBackground();
    glutDisplayFunc(display);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(specialKeys);
    glutSpecialUpFunc(specialKeysUp);
    glutTimerFunc(1000 / FPS, update, 0);
    glutMainLoop();
    return 0;
}