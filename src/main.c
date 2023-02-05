#include <genesis.h>
#include <resources.h>

#define MAX_ENEMIES 6
#define MAX_BULLETS 6
#define MAX_PLAYER_BULLETS 3

#define LEFT_EDGE 0
#define RIGHT_EDGE 320
#define BOTTOM_EDGE 224

#define POWERUP_DURATION 180
#define SHOT_INTERVAL 120

#define ANIM_STRAIGHT 0
#define ANIM_MOVE 1

#define SFX_LASER 64
#define SFX_EXPLOSION 65

typedef struct {
  int x;
  int y;
  int w;
  int h;
  int velx;
  int vely;
  int health;
  Sprite *sprite;
  char name[7];
} Entity;

typedef enum { RapidFire } Powerup;

Entity player = {0, 0, 16, 16, 0, 0, 0, NULL, "PLAYER\0"};
Entity powerup = {0, 0, 8, 8, 0, 0, 0, NULL};
Entity enemies[MAX_ENEMIES];
Entity bullets[MAX_BULLETS];
u16 enemiesLeft = 0;
u16 bulletsOnScreen = 0;
u16 maxPlayerBullets = MAX_PLAYER_BULLETS;
u16 powerupTimer = 0;

int score = 0;
char hud_string[40] = "";
u16 shotTicker = 0;
u16 shotByPlayer = 0;

// Handle Entities
void killEntity(Entity *e) {
  e->health = 0;
  SPR_setVisibility(e->sprite, HIDDEN);
}

void reviveEntity(Entity *e) {
  e->health = 1;
  SPR_setVisibility(e->sprite, VISIBLE);
}

void shootBullet(Entity Shooter) {
  bool fromPlayer = (Shooter.y > 100);
  if (fromPlayer == TRUE) {
    if (shotByPlayer >= maxPlayerBullets) {
      return;
    }
  }

  if (bulletsOnScreen < MAX_BULLETS) {
    Entity *b;
    u16 i = 0;
    for (i = 0; i < MAX_BULLETS; i++) {
      b = &bullets[i];
      if (b->health == 0) {
        b->x = Shooter.x + 4;
        b->y = Shooter.y;

        reviveEntity(b);
        if (fromPlayer == TRUE) {
          b->vely = -3;
          shotByPlayer++;
        } else {
          b->vely = 3;
        }

        SPR_setPosition(b->sprite, b->x, b->y);
        bulletsOnScreen++;
        XGM_startPlayPCM(SFX_LASER, 1, SOUND_PCM_CH2);
        break;
      }
    }
  }
}

void positionEnemies() {
  shotTicker++;
  u16 i = 0;

  for (i = 0; i < MAX_ENEMIES; i++) {
    Entity *e = &enemies[i];
    if (e->health > 0) {
      if ((e->x + e->w) > RIGHT_EDGE) {
        e->velx = -1;
      } else if (e->x < LEFT_EDGE) {
        e->velx = 1;
      }
      e->x += e->velx;
      SPR_setPosition(e->sprite, e->x, e->y);

      if (shotTicker >= SHOT_INTERVAL) {
        if (((random() % 10) + 1) > 4) {
          shootBullet(*e);
          shotTicker = 0;
        }
      }
    }
  }
}

void positionPlayer() {
  /*Add the player's velocity to its position*/
  player.x += player.velx;

  /*Keep the player within the bounds of the screen*/
  if (player.x < LEFT_EDGE)
    player.x = LEFT_EDGE;
  if (player.x + player.w > RIGHT_EDGE)
    player.x = RIGHT_EDGE - player.w;

  /*Let the Sprite engine position the sprite*/
  SPR_setPosition(player.sprite, player.x, player.y);
}

void positionBullets() {
  u16 i = 0;
  Entity *b;
  for (i = 0; i < MAX_BULLETS; i++) {
    b = &bullets[i];
    if (b->health > 0) {
      b->y += b->vely;
      if (b->y + b->h < 0) {
        killEntity(b);
        bulletsOnScreen--;
        shotByPlayer--;
      } else if (b->y > BOTTOM_EDGE) {
        killEntity(b);
        bulletsOnScreen--;
      } else {
        SPR_setPosition(b->sprite, b->x, b->y);
      }
    }
  }
}

void updateScoreDisplay() {
  sprintf(hud_string, "SCORE: %d - LEFT: %d", score, enemiesLeft);
  VDP_clearText(0, 0, 40);
  VDP_drawText(hud_string, 0, 0);
}

int collideEntities(Entity *a, Entity *b) {
  return (a->x < b->x + b->w && a->x + a->w > b->x && a->y < b->y + b->h &&
          a->y + a->h >= b->y);
}

void spawnPowerupAt(u16 X, u16 Y) {
  powerup.x = X;
  powerup.y = Y;
  reviveEntity(&powerup);
}

void activatePowerup(Powerup Type) {
  switch (Type) {
  case RapidFire: {
    maxPlayerBullets = MAX_BULLETS;
    powerupTimer = POWERUP_DURATION;
    VDP_setPaletteColor(18, RGB24_TO_VDPCOLOR(0xf8fc00));
  }
  default:
    return;
  }
}

void positionPowerup() {
  if (powerup.health > 0) {
    powerup.y++;

    if (powerup.y > BOTTOM_EDGE) {
      killEntity(&powerup);
    }

    if (collideEntities(&player, &powerup)) {
      activatePowerup(RapidFire);
      killEntity(&powerup);
    }
  }

  SPR_setPosition(powerup.sprite, powerup.x, powerup.y);
}

void deactivatePowerup() {
  maxPlayerBullets = MAX_PLAYER_BULLETS;
  VDP_setPaletteColor(18, RGB24_TO_VDPCOLOR(0xf83800));
}

void handleCollisions() {
  Entity *b;
  Entity *e;
  int i = 0;
  int j = 0;

  for (i = 0; i < MAX_BULLETS; i++) {
    b = &bullets[i];
    if (b->health > 0) {
      if (b->vely < 0) {
        for (j = 0; j < MAX_ENEMIES; j++) {
          e = &enemies[j];
          if (e->health > 0) {
            if (collideEntities(b, e)) {
              killEntity(e);
              killEntity(b);
              XGM_startPlayPCM(SFX_EXPLOSION, 1, SOUND_PCM_CH3);

              enemiesLeft--;
              bulletsOnScreen--;
              shotByPlayer--;

              score += 10;
              updateScoreDisplay();

              if (enemiesLeft % 5 == 0) {
                spawnPowerupAt(e->x, e->y);
              }

              break;
            }
          }
        }
      } else {
        if (collideEntities(b, &player)) {
          killEntity(&player);
          XGM_startPlayPCM(SFX_EXPLOSION, 1, SOUND_PCM_CH3);
        }
      }
    }
  }
}

// Joystick Handler
void myJoyHandler(u16 joy, u16 changed, u16 state) {
  if (joy == JOY_1) {
    if (state & BUTTON_RIGHT) {
      player.velx = 2;
      SPR_setAnim(player.sprite, ANIM_MOVE);
      SPR_setHFlip(player.sprite, TRUE);
    } else if (state & BUTTON_LEFT) {
      player.velx = -2;
      SPR_setAnim(player.sprite, ANIM_MOVE);
      SPR_setHFlip(player.sprite, FALSE);
    } else {
      if ((changed & BUTTON_RIGHT) | (changed & BUTTON_LEFT)) {
        player.velx = 0;
        SPR_setAnim(player.sprite, ANIM_STRAIGHT);
      }
    }

    if (state & BUTTON_B) {
      shootBullet(player);
    }
  }
}

int main() {
  // Init system
  JOY_init();
  JOY_setEventHandler(&myJoyHandler);
  // Initialize the sound to the sound driver
  XGM_setPCM(SFX_LASER, sfx_laser, sizeof(sfx_laser));
  XGM_setPCM(SFX_EXPLOSION, sfx_explosion, sizeof(sfx_explosion));
  SPR_init();
  SYS_disableInts();

  // Code that loads the tile, draws the background...
  // Always wrap VDP_* functions with SYS_disableInts() and SYS_enableInts()
  VDP_loadTileSet(background.tileset, 1, DMA);
  VDP_setPalette(PAL1, background.palette->data);
  VDP_setPalette(PAL2, background.palette->data);

  // Generate background
  int i = 0;
  int thex = 0;
  int they = 0;
  int val = 1;
  for (i = 0; i < 1280; i++) {
    thex = i % 40;
    they = i / 40;

    // (random() % (max - min + 1)) + min | max = 10, min = 1
    val = (random() % 10) + 1;
    if (val > 3)
      val = 1;

    VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL1, 0, 0, 0, val), thex, they);
  }

  /*Add the player*/
  player.x = 152;
  player.y = 192;
  player.health = 1;
  player.sprite = SPR_addSprite(&ship, player.x, player.y,
                                TILE_ATTR(PAL1, 0, FALSE, FALSE));

  /*Create all enemy sprites*/
  Entity *e = enemies;
  for (i = 0; i < MAX_ENEMIES; i++) {
    e->x = i * 32;
    e->y = 32;
    e->w = 16;
    e->h = 16;
    e->velx = 1;
    e->health = 1;
    e->sprite =
        SPR_addSprite(&ship, e->x, e->y, TILE_ATTR(PAL2, 0, TRUE, FALSE));
    sprintf(e->name, "En%d", i);
    enemiesLeft++;
    e++;
  }
  VDP_setPaletteColor(34, RGB24_TO_VDPCOLOR(0x0078f8));

  /*Create all bullet sprites*/
  Entity *b = bullets;
  for (i = 0; i < MAX_BULLETS; i++) {
    b->x = 0;
    b->y = -10;
    b->w = 8;
    b->h = 8;
    b->sprite = SPR_addSprite(&bullet, bullets[0].x, bullets[0].y,
                              TILE_ATTR(PAL1, 0, FALSE, FALSE));
    sprintf(b->name, "Bu%d", i);
    b++;
  }

  /*Create the powerup*/
  powerup.sprite = SPR_addSprite(&spr_powerup, powerup.x, powerup.y,
                                 TILE_ATTR(PAL1, 0, FALSE, FALSE));
  killEntity(&powerup);

  updateScoreDisplay();

  SPR_update();

  int offset = 0;
  VDP_setScrollingMode(HSCROLL_PLANE, VSCROLL_PLANE);
  SYS_enableInts();

  while (1) {
    SYS_disableInts();

    VDP_setVerticalScroll(BG_B, offset -= 2);
    if (offset <= -256)
      offset = 0;

    positionPlayer();
    positionBullets();
    positionEnemies();
    positionPowerup();

    handleCollisions();

    if (powerupTimer > 0) {
      powerupTimer--;
      if (powerupTimer == 0) {
        deactivatePowerup();
      }
    }

    SPR_update();
    SYS_enableInts();
    SYS_doVBlankProcess();
  }
  return (0);
}
