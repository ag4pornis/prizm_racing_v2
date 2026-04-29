#if !defined(GINT) && !defined(PRIZM) && !defined(SDL)
#define PRIZM
#endif

// Compatibilidad para el editor de código
#ifndef LCD_WIDTH_PX
#define LCD_WIDTH_PX 384
#define LCD_HEIGHT_PX 216
#define KEY_PRGM_MENU 0
#define KEY_PRGM_F1 0
#define KEY_PRGM_EXE 0
#endif

#include "display.h"
#include "input.h"
#include "mat4.h"
#include "math.h"
#include "rasterizer.h"
#include "rmath.h"
#include "time.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "car.h"
#include "track.h"

#ifdef GINT
#include <gint/dma.h>
#include <gint/gint.h>
#include <gint/mmu.h>
#endif

#ifdef PRIZM
#include <fxcg/serial.h>
#include <fxcg/system.h>
int timer;
#endif

mat4 view;
vec3<float> cameraPos = {0, 0, 0};
vec3<float> cameraSpeed = {0, 0, 0};
float cameraAngle = 0;

enum GameState { FREE_ROAM, WAITING_FOR_OPPONENT, COUNTDOWN, RACING, FINISHED };

enum GameMode { MODE_MULTI, MODE_TIME_TRIAL };

GameState gameState = FREE_ROAM;
GameMode gameMode = MODE_MULTI;
int currentLap = 0;
int nextCheckpoint = 0;
float raceCountdown = 0;
float currentLapTime = 0;
float totalRaceTime = 0;
float lapRecords[5] = {0};
float bestLaps[3] = {1e9, 1e9, 1e9};
bool isWinner = false;
bool raceRequested = false;
bool opponentRaceRequested = false;
bool hasFinished = false;
bool opponentHasFinished = false;

#ifdef GINT
static GALIGNED(32) unsigned char depthBuffer[RENDER_WIDTH * RENDER_HEIGHT];
#include "models.h"
#include "textures.h"
#endif

int main() {
#ifndef GINT
  unsigned char depthBuffer[RENDER_WIDTH * RENDER_HEIGHT];
#include "models.h"
#include "textures.h"
#endif
  Rasterizer::depthBuffer = depthBuffer;

  createSinTable();

  Car::model = Model({181, car_triangles}, &car_texture);
  Track::coneMesh = {43, cone_triangles};

  Time::init();

  Display::init();
  Display::clear(newColor(70, 180, 220));
  Display::show();

  Time::update();

  Input::init();

  srand(0);

  Model sun({8, sun_triangles});

  vec3<float> trackPoints[] = {
      {0, 0, 0},
      {2, 0, 0},
      {2 + I_SQRT_2, 0, 1 - I_SQRT_2},
      {3, 0, 1},
      {4 - I_SQRT_2, 0, 1 + I_SQRT_2},
      {4, 0, 2},
      {4 + I_SQRT_2, 0, 1 + I_SQRT_2},
      {5, 0, 1},
      {6 - I_SQRT_2, 0, 1 - I_SQRT_2},
      {6, 0, 0},
      {7, 0, 0},
      {7 + I_SQRT_2, 0, -1 + I_SQRT_2},
      {8, 0, -1},
      {7 + I_SQRT_2, 0, -1 - I_SQRT_2},
      {7, 0, -2},
      {5, 0, -2},
      {5 - I_SQRT_2, 0, -3 + I_SQRT_2},
      {4, 0, -3},
      {3 + I_SQRT_2, 0, -3 - I_SQRT_2},
      {3, 0, -4},
      {3 - I_SQRT_2, 0, -3 - I_SQRT_2},
      {2, 0, -3},
      {1 + I_SQRT_2, 0, -3 + I_SQRT_2},
      {1, 0, -2},
      {0, 0, -2},
      {-I_SQRT_2, 0, -1 - I_SQRT_2},
      {-1, 0, -1},
      {-I_SQRT_2, 0, -1 + I_SQRT_2},
  };
  for (int i = 0; i < 28; i++) {
    trackPoints[i] = trackPoints[i] * 40;
    trackPoints[i].y = 0;
  }
  Track track = Track(28, trackPoints, 10, 1.0);

  float minX = 1e9, maxX = -1e9, minZ = 1e9, maxZ = -1e9;
  for (int i = 0; i < 28; i++) {
    if (trackPoints[i].x < minX)
      minX = trackPoints[i].x;
    if (trackPoints[i].x > maxX)
      maxX = trackPoints[i].x;
    if (trackPoints[i].z < minZ)
      minZ = trackPoints[i].z;
    if (trackPoints[i].z > maxZ)
      maxZ = trackPoints[i].z;
  }

  Rasterizer::init();
  Rasterizer::setFOV(70);

  Car car = Car();
#ifdef PRIZM
  Car enemyCar = Car();
  enemyCar.position = {-1000, 0, 0};
#endif

#ifdef PRIZM
  Serial_Close(1);
  while (Serial_IsOpen() != 1) {
    unsigned char mode[6] = {0, 5, 0, 0, 0, 0}; // 9600 bps 8n1
    Serial_Open(mode);
  }
  Serial_ClearTX();
  Serial_ClearRX();
#endif

  while (true) {
    Rasterizer::reset();

    Time::update();
    Input::update();

#ifdef PRIZM
    // The first 4 bits of a sent byte are 0b0001 if it's the first being sent
    // and 0b0000 otherwise. The last 4 bits contain data. Each sent byte
    // contains half a byte of data.

    while (Serial_PollRX() > 0) {
      unsigned char o;
      Serial_Peek(0, &o);
      if (!(o & (1 << 4))) {
        Serial_ReadSingle(NULL);
      } else {
        opponentRaceRequested = (o & (1 << 5)) != 0;
        opponentHasFinished = (o & (1 << 6)) != 0;
        if (Serial_PollRX() >= sizeof(Car) * 2) {
          unsigned char enemyCarData[sizeof(Car) * 2];
          Serial_Read(enemyCarData, sizeof(Car) * 2, NULL);
          for (int i = 0; i < sizeof(Car); i++) {
            *(((unsigned char *)&enemyCar) + i) =
                ((enemyCarData[i * 2] & 0x0F) << 4) |
                (enemyCarData[i * 2 + 1] & 0x0F);
          }
        }
        break;
      }
    }

    if (Serial_PollTX() >= sizeof(Car) * 2) {
      unsigned char carData[sizeof(Car) * 2];
      for (int i = 0; i < sizeof(Car); i++) {
        carData[i * 2] = ((*(((unsigned char *)&car) + i)) & 0xF0) >> 4;
        carData[i * 2 + 1] = ((*(((unsigned char *)&car) + i)) & 0x0F);
      }
      carData[0] = carData[0] | (1 << 4);
      if (raceRequested)
        carData[0] |= (1 << 5);
      if (hasFinished)
        carData[0] |= (1 << 6);
      Serial_Write(carData, sizeof(Car) * 2);
    }
#endif

    if (Input::keyPressed(KEY_MENU)) {
#ifdef GINT
      gint_osmenu();
#endif
#ifdef PRIZM
      while (Input::keyDown(KEY_MENU))
        Input::update();

      Serial_Close(1);

      timer = Timer_Install(
          0,
          []() {
            Keyboard_PutKeycode(4, 9, 0);
            Timer_Stop(timer);
            Timer_Deinstall(timer);
          },
          1);
      Timer_Start(timer);
      int k = 0;
      Bdisp_EnableColor(1);
      GetKey(&k);

      Serial_Close(1);
      while (Serial_IsOpen() != 1) {
        unsigned char mode[6] = {0, 5, 0, 0, 0, 0}; // 9600 bps 8n1
        Serial_Open(mode);
      }
      Serial_ClearTX();
      Serial_ClearRX();

      continue;
#endif
#ifdef SDL
      return 0;
#endif
      int t = Time::delta;
      Time::update();
      Time::delta = t;
    }

    if (Input::keyPressed(KEY_F1) && gameState == FREE_ROAM) {
      raceRequested = true;
      gameMode = MODE_MULTI;
      gameState = WAITING_FOR_OPPONENT;
    }

    if (Input::keyPressed(KEY_F2) && gameState == FREE_ROAM) {
      gameMode = MODE_TIME_TRIAL;
      gameState = COUNTDOWN;
      raceCountdown = 3.0f;
      currentLap = 0;
      nextCheckpoint = 0;
      currentLapTime = 0;
      totalRaceTime = 0;
      car.position = {0, 0, 0};
      car.speed = {0, 0, 0};
      car.wheelSpeed = 0;
      car.direction = 0;
      car.targetDirection = 0;
      hasFinished = false;
      opponentHasFinished = false;
    }

    if (gameState == WAITING_FOR_OPPONENT) {
      if (opponentRaceRequested) {
        gameState = COUNTDOWN;
        raceCountdown = 3.0f;
        currentLap = 0;
        nextCheckpoint = 0;
        currentLapTime = 0;
        totalRaceTime = 0;
        car.position = {0, 0, 0};
        car.speed = {0, 0, 0};
        car.wheelSpeed = 0;
        car.direction = 0;
        car.targetDirection = 0;
        hasFinished = false;
        opponentHasFinished = false;
      }
    }

    if (gameState == COUNTDOWN) {
#ifdef PRIZM
      raceCountdown -= Time::delta / 128.0f;
#else
      raceCountdown -= Time::delta / 1000.0f;
#endif
      if (raceCountdown <= 0) {
        gameState = RACING;
      }
    }

    if (gameState == RACING) {
#ifdef PRIZM
      currentLapTime += Time::delta / 128.0f;
      totalRaceTime += Time::delta / 128.0f;
#else
      currentLapTime += Time::delta / 1000.0f;
      totalRaceTime += Time::delta / 1000.0f;
#endif

      if (gameMode == MODE_MULTI && opponentHasFinished) {
        gameState = FINISHED;
        isWinner = false;
        car.position = {20, 0, 0};
        car.speed = {0, 0, 0};
        car.wheelSpeed = 0;
        car.direction = 0;
        enemyCar.position = {-20, 0, 0};
        enemyCar.direction = 0;
      } else {
        vec3<float> cpPos = track.getPoint(nextCheckpoint);
        if ((car.position - cpPos).length2() < 20 * 20 &&
            track.isInside(car.position)) {
          nextCheckpoint++;
          if (nextCheckpoint >= track.getNumPoints()) {
            nextCheckpoint = 0;
            if (currentLap < 5)
              lapRecords[currentLap] = currentLapTime;

            // Update session bests
            for (int i = 0; i < 3; i++) {
              if (currentLapTime < bestLaps[i]) {
                for (int j = 2; j > i; j--)
                  bestLaps[j] = bestLaps[j - 1];
                bestLaps[i] = currentLapTime;
                break;
              }
            }

            currentLapTime = 0;
            currentLap++;
            if (currentLap >= 5) {
              gameState = FINISHED;
              isWinner = true;
              hasFinished = true;
              car.position = {20, 0, 0};
              car.speed = {0, 0, 0};
              car.wheelSpeed = 0;
              car.direction = 0;
              enemyCar.position = {-20, 0, 0};
              enemyCar.direction = 0;
            }
          }
        }
      }
    }

    if (gameState != COUNTDOWN && gameState != FINISHED &&
        gameState != WAITING_FOR_OPPONENT)
      car.processInput();

    car.update(track.isInside(car.position));
#ifdef PRIZM
    enemyCar.update(track.isInside(enemyCar.position));
#endif

    Rasterizer::setFOV(fp(70 + 50.0f / car.speed.i_length()));

    cameraAngle = cameraAngle +
                  (-cameraAngle * 0.02 + 0.02 * car.direction) * Time::delta;

    vec3<float> lc = cameraPos;
    vec3<float> ls = cameraSpeed;
    cameraPos = smoothDamp(cameraPos, car.position, &cameraSpeed, 5.0f,
                           Time::delta, 1000.0f);
    if (cameraPos == car.position) {
      cameraPos = lc;
      cameraSpeed = ls;
    }

    view = mat4();
    view = mat4::rotateX(view, HALF_PI * 0.1);
    view = mat4::translate(view, 0, 2, 6);
    view = mat4::rotateY(view, -cameraAngle - HALF_PI);
    view = mat4::translate(view, -cameraPos.x, 0, -cameraPos.z);

    int floorY =
        fp_tan(-HALF_PI * 0.1) / Rasterizer::fov_d * fp(DISPLAY_WIDTH) +
        fp(DISPLAY_HEIGHT / 2);
#ifdef PRIZM
    long v1 = newColor(70, 180, 220).color |
              (newColor(70, 180, 220).color << (8 * sizeof(unsigned short)));
    long v2 = newColor(32, 79, 19).color |
              (newColor(32, 79, 19).color << (8 * sizeof(unsigned short)));
    for (int i = 0; i < floorY * DISPLAY_WIDTH / 2; i++) {
      ((long *)Display::VRAMAddress)[i] = v1;
    }
    for (int i = floorY * DISPLAY_WIDTH / 2;
         i < DISPLAY_WIDTH * DISPLAY_HEIGHT / 2; i++) {
      ((long *)Display::VRAMAddress)[i] = v2;
    }
#else
#ifdef GINT
    uint16_t sky = newColor(70, 180, 220).color;
    uint16_t grass = newColor(32, 79, 10).color;
    int minY = floorY - (floorY % 4);
    int maxY = floorY + (4 - (floorY % 4));
    dma_memset(gint_vram, (sky << 16) | sky, DISPLAY_WIDTH * minY * 2);
    Display::fillRect(0, minY, DISPLAY_WIDTH, floorY - minY,
                      newColor(70, 180, 220));
    Display::fillRect(0, floorY, DISPLAY_WIDTH, maxY - floorY,
                      newColor(32, 79, 10));
    dma_memset(gint_vram + DISPLAY_WIDTH * maxY, (grass << 16) | grass,
               DISPLAY_WIDTH * DISPLAY_HEIGHT * 2 - DISPLAY_WIDTH * maxY * 2);
#else
    Display::clear(newColor(70, 180, 220));
    Display::fillRect(0, floorY, DISPLAY_WIDTH, DISPLAY_HEIGHT - floorY,
                      newColor(32, 79, 19));
#endif
#endif

    sun.viewMatrix = view;
    sun.modelMatrix = mat4();
    sun.modelMatrix = mat4::translate(sun.modelMatrix, 20, -6, -20);
    sun.modelMatrix =
        mat4::translate(sun.modelMatrix, cameraPos.x, 0, cameraPos.z);
    sun.modelMatrix = mat4::rotateY(sun.modelMatrix, cameraAngle + HALF_PI);
    sun.draw(false, false, true, true);

    track.render(view, car.position);

#ifdef PRIZM
    enemyCar.render(view);
#endif
    car.render(view);

    char buffer[20];
#ifdef PRIZM
    itoa((int)(1.0f / (Time::delta / 128.0f)), (unsigned char *)buffer);
#else
    sprintf(buffer, "%d", (int)(1.0f / (Time::delta / 128.0f)));
#endif
    Display::drawText(0, 0, "FPS: ", newColor(255, 255, 255));
    Display::drawText(Display::textWidth("FPS: "), 0, buffer,
                      newColor(255, 255, 255));

    float speed2 = car.speed.length2();
    float speed;
    if (speed2 != 0)
      speed = (1.0f / car.speed.i_length()) * 128.0f / 1000.0f * 3600.0f;
    else
      speed = 0;
#ifdef PRIZM
    itoa((int)speed, (unsigned char *)buffer);
#else
    sprintf(buffer, "%d", (int)speed);
#endif
    Display::drawText(0, DISPLAY_HEIGHT - Display::textHeight,
                      "SPEED: ", newColor(255, 255, 255));
    Display::drawText(Display::textWidth("SPEED: "),
                      DISPLAY_HEIGHT - Display::textHeight, buffer,
                      newColor(255, 255, 255));

    if (gameState == WAITING_FOR_OPPONENT) {
      Display::drawText(
          DISPLAY_WIDTH / 2 - Display::textWidth("WAITING FOR OPPONENT") / 2,
          DISPLAY_HEIGHT / 2, "WAITING FOR OPPONENT", newColor(255, 255, 255));
    }

    if (gameState == COUNTDOWN) {
      int dots = (int)raceCountdown + 1;
      for (int i = 0; i < 3; i++) {
        Color c =
            (i < (3 - dots + 1)) ? newColor(255, 0, 0) : newColor(50, 0, 0);
        Display::fillRect(DISPLAY_WIDTH / 2 - 50 + i * 40, 50, 20, 20, c);
      }
    }

    if (gameState == RACING) {
      // Wrong Way detection with filter
      vec3<float> p0 = track.getPoint(
          (nextCheckpoint - 1 + track.getNumPoints()) % track.getNumPoints());
      vec3<float> p1 = track.getPoint(nextCheckpoint);
      vec3<float> trackDir = p1 - p0;
      vec3<float> carForward = {(float)fp_cos(fp(car.direction)), 0,
                                -(float)fp_sin(fp(car.direction))};

      float mag =
          1.0f / _isqrt(trackDir.x * trackDir.x + trackDir.z * trackDir.z);
      float dotNorm = (carForward.x * trackDir.x + carForward.z * trackDir.z) /
                      (mag + 0.001f);

      static float wrongWayTimer = 0;
      if (dotNorm < -0.6f && car.speed.length2() > 5.0f) {
#ifdef PRIZM
        wrongWayTimer += Time::delta / 128.0f;
#else
        wrongWayTimer += Time::delta / 1000.0f;
#endif
      } else {
        wrongWayTimer = 0;
      }

      if (wrongWayTimer > 0.5f) {
        Display::drawText(
            DISPLAY_WIDTH / 2 - Display::textWidth("WRONG WAY") / 2,
            DISPLAY_HEIGHT / 2 + 40, "WRONG WAY", newColor(255, 0, 0));
      }

#ifdef PRIZM
      itoa(currentLap + 1, (unsigned char *)buffer);
#else
      sprintf(buffer, "%d", currentLap + 1);
#endif
      Display::drawText(0, 20, "LAP: ", newColor(255, 255, 0));
      Display::drawText(Display::textWidth("LAP: "), 20, buffer,
                        newColor(255, 255, 0));
      Display::drawText(Display::textWidth("LAP: ") +
                            Display::textWidth(buffer),
                        20, "/5", newColor(255, 255, 0));

      Display::drawText(0, 40, "CP: ", newColor(255, 255, 0));
      int cpX = Display::textWidth("CP: ");
#ifdef PRIZM
      itoa(nextCheckpoint, (unsigned char *)buffer);
#else
      sprintf(buffer, "%d", nextCheckpoint);
#endif
      Display::drawText(cpX, 40, buffer, newColor(255, 255, 0));
      cpX += Display::textWidth(buffer);
      Display::drawText(cpX, 40, "/", newColor(255, 255, 0));
      cpX += Display::textWidth("/");
#ifdef PRIZM
      itoa(track.getNumPoints(), (unsigned char *)buffer);
#else
      sprintf(buffer, "%d", track.getNumPoints());
#endif
      Display::drawText(cpX, 40, buffer, newColor(255, 255, 0));

      int m = (int)(currentLapTime / 60);
      float s = currentLapTime - m * 60;
#ifdef PRIZM
      itoa(m, (unsigned char *)buffer);
#else
      sprintf(buffer, "%d", m);
#endif
      Display::drawText(0, 60, "TIME: ", newColor(255, 255, 255));
      Display::drawText(Display::textWidth("TIME: "), 60, buffer,
                        newColor(255, 255, 255));
      Display::drawText(Display::textWidth("TIME: ") +
                            Display::textWidth(buffer),
                        60, ":", newColor(255, 255, 255));
#ifdef PRIZM
      if ((int)s < 10)
        strcat(buffer, "0"); // Manual padding for Prizm
      char sBuf[10];
      itoa((int)s, (unsigned char *)sBuf);
      if ((int)s < 10) {
        Display::drawText(Display::textWidth("TIME: ") +
                              Display::textWidth("0:"),
                          60, "0", newColor(255, 255, 255));
        Display::drawText(Display::textWidth("TIME: ") +
                              Display::textWidth("0:0"),
                          60, sBuf, newColor(255, 255, 255));
      } else {
        Display::drawText(Display::textWidth("TIME: ") +
                              Display::textWidth("0:"),
                          60, sBuf, newColor(255, 255, 255));
      }
#else
      sprintf(buffer, "%02d", (int)s);
      Display::drawText(Display::textWidth("TIME: ") + Display::textWidth("0:"),
                        60, buffer, newColor(255, 255, 255));
#endif
    }

    if (gameState == FINISHED) {
      if (gameMode == MODE_MULTI) {
        if (isWinner)
          Display::drawText(DISPLAY_WIDTH / 2 -
                                Display::textWidth("WINNER!") / 2,
                            20, "WINNER!", newColor(0, 255, 0));
        else
          Display::drawText(DISPLAY_WIDTH / 2 -
                                Display::textWidth("YOU LOST") / 2,
                            20, "YOU LOST", newColor(255, 0, 0));
      } else {
        Display::drawText(DISPLAY_WIDTH / 2 -
                              Display::textWidth("TIME TRIAL FINISHED") / 2,
                          10, "TIME TRIAL FINISHED", newColor(255, 255, 255));
      }

      // Show Lap Summary
      for (int i = 0; i < 5; i++) {
        int lm = (int)(lapRecords[i] / 60);
        float ls = lapRecords[i] - lm * 60;
        char lapBuf[32];
#ifdef PRIZM
        char sBuf[10];
        itoa(i + 1, (unsigned char *)lapBuf);
        strcat(lapBuf, ": ");
        itoa(lm, (unsigned char *)sBuf);
        strcat(lapBuf, sBuf);
        strcat(lapBuf, ":");
        itoa((int)ls, (unsigned char *)sBuf);
        strcat(lapBuf, sBuf);
#else
        sprintf(lapBuf, "LAP %d: %d:%02d", i + 1, lm, (int)ls);
#endif
        Display::drawText(20, 50 + i * 15, lapBuf, newColor(255, 255, 255));
      }

      // Show session records if in Time Trial
      if (gameMode == MODE_TIME_TRIAL) {
        Display::drawText(200, 50, "BEST LAPS:", newColor(255, 255, 0));
        for (int i = 0; i < 3; i++) {
          if (bestLaps[i] > 3600)
            continue;
          int bm = (int)(bestLaps[i] / 60);
          float bs = bestLaps[i] - bm * 60;
          char bestBuf[32];
#ifdef PRIZM
          itoa(i + 1, (unsigned char *)bestBuf);
          strcat(bestBuf, ". ");
          char sBuf[10];
          itoa(bm, (unsigned char *)sBuf);
          strcat(bestBuf, sBuf);
          strcat(bestBuf, ":");
          itoa((int)bs, (unsigned char *)sBuf);
          strcat(bestBuf, sBuf);
#else
          sprintf(bestBuf, "%d. %d:%02d", i + 1, bm, (int)bs);
#endif
          Display::drawText(200, 70 + i * 15, bestBuf, newColor(255, 255, 255));
        }
      }

      Display::drawText(
          DISPLAY_WIDTH / 2 - Display::textWidth("PRESS EXE TO RETURN") / 2,
          DISPLAY_HEIGHT - 20, "PRESS EXE TO RETURN", newColor(150, 150, 150));

      if (Input::keyPressed(KEY_EXE)) {
        gameState = FREE_ROAM;
        raceRequested = false;
      }
    }

    if (gameState == FREE_ROAM) {
      Display::drawText(0, 20, "F1: CARRERA (MULTI)", newColor(255, 255, 255));
      Display::drawText(0, 40, "F2: CONTRARRELOJ (SOLO)",
                        newColor(255, 255, 255));
    }

    // Minimap
    int mapW = 60;
    int mapH = 40;
    int mapX = DISPLAY_WIDTH - mapW - 10;
    int mapY = 10;
    Display::fillRect(mapX - 2, mapY - 2, mapW + 4, mapH + 4,
                      newColor(0, 0, 0)); // Background

    for (int i = 0; i < 28; i++) {
      int x = mapX + (int)((trackPoints[i].x - minX) / (maxX - minX) * mapW);
      int y = mapY + (int)((trackPoints[i].z - minZ) / (maxZ - minZ) * mapH);
      Display::drawPoint(x, y, newColor(100, 100, 100));
    }

    int pX = mapX + (int)((car.position.x - minX) / (maxX - minX) * mapW);
    int pY = mapY + (int)((car.position.z - minZ) / (maxZ - minZ) * mapH);
    if (pX < mapX)
      pX = mapX;
    if (pX > mapX + mapW)
      pX = mapX + mapW;
    if (pY < mapY)
      pY = mapY;
    if (pY > mapY + mapH)
      pY = mapY + mapH;
    Display::fillRect(pX - 1, pY - 1, 3, 3, newColor(255, 255, 255));

#ifdef PRIZM
    int eX = mapX + (int)((enemyCar.position.x - minX) / (maxX - minX) * mapW);
    int eY = mapY + (int)((enemyCar.position.z - minZ) / (maxZ - minZ) * mapH);
    if (eX < mapX)
      eX = mapX;
    if (eX > mapX + mapW)
      eX = mapX + mapW;
    if (eY < mapY)
      eY = mapY;
    if (eY > mapY + mapH)
      eY = mapY + mapH;
    Display::fillRect(eX - 1, eY - 1, 3, 3, newColor(255, 0, 0));
#endif

    Display::show();
  }
}
