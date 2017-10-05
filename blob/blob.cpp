#include "spritegroup.cpp"
#include "math.cpp"

const int BLOB_MAX_BOUNCES = 5;
const int BLOB_COLLISION_CHECKS = 7;
const float BLOB_BASE_RADIUS = 26;
const float BLOB_BOUNCE_ANGULAR_FRICTION = 0.5;
const float BLOB_BOUNCE_EFFICIENCY = 0.85;
const float BLOB_MOMENT_OF_INERTIA = 1.0;
const float BLOB_MAX_ANGULAR_MOMENTUM = 4000;
const float BLOB_STICK_SPEED_THRESH = 10;
const float BLOB_DASH_ANGULAR_SPEED = 720;
const float BLOB_BOUNCE_DI_FORCE = 200;
const float BLOB_AIR_FRICTION = 0.85; /* Decay/s */
const float BLOB_AIR_FRICTION_DI_COEFF = 0.4;

enum blob_state {
  blob_state_roll = 0,
  blob_state_dash = 1,
  blob_state_jump = 2,
}

class tile_clean_data {
  int x;
  int y;
  float timer;

  tile_clean_data() {
    x = 0;
    y = 0;
    timer = 0;
  }

  tile_clean_data(int x, int y, float timer) {
    this.x = x;
    this.y = y;
    this.timer = timer;
  }
}

class blob : enemy_base, callback_base {
  scene@ g;
  scriptenemy@ self;
  collision@ player_collision;
  hitbox@ attack_hitbox;
  sprite_group spr;

  [slider,min:0,max:4000]
  float gravity;

  [hidden] float angular_momentum;
  [hidden] int state;
  [hidden] float state_timer;

  float prev_x;
  float prev_y;

  array<tile_clean_data> clean_tiles;

  blob() {
    @g = get_scene();
    spr.add_sprite("beachball", "beachball");

    gravity = 1500;
    angular_momentum = 0;
    state = 0;
  }

  void init(script@ sc, scriptenemy@ self) {
    @this.self = @self;
    @player_collision = g.add_collision(@self.as_entity(), -1, 1, -1, 1,
                                        5 /* col_type_player */);
    prev_x = self.x();
    prev_y = self.y();

    self.auto_physics(false);
    self.on_hit_callback(@this, "on_hit", 0);
    self.on_hurt_callback(@this, "on_hurt", 0);
  }

  void on_hit(controllable@ attacker, controllable@ attacked,
              hitbox@ hb, int) {
    if (hb.damage() == 3) {
      float force = hb.attack_strength();
      float dir = point_angle(attacked.x(), attacker.y(), self.x(), self.y());
      float x_speed = self.x_speed() + lengthdir_x(force, dir);
      float y_speed = self.y_speed() + lengthdir_y(force, dir);
      self.set_speed_xy(x_speed, y_speed);
    }
  }

  void on_hurt(controllable@ attacker, controllable@ attacked,
               hitbox@ hb, int) {
    float force = hb.attack_strength();
    float dir = hb.attack_dir();
    if (hb.aoe()) {
      dir = point_angle(hb.x(), hb.y(), self.x(), self.y());
    }
    float x_speed = self.x_speed() + lengthdir_x(force, dir);
    float y_speed = self.y_speed() + lengthdir_y(force, dir);
    self.set_speed_xy(x_speed, y_speed);
    break_combo();
  }

  float inc(float x) {
    return x / 60.0 * self.time_warp();
  }

  float s(float x) {
    return x * self.scale();
  }

  float s_inc(float x) {
    return s(inc(x));
  }

  void state_roll() {
    if (@attack_hitbox == null &&
        0 < self.light_intent() && self.light_intent() <= 10) {
      self.light_intent(11);
      @attack_hitbox = create_hitbox(@self.as_controllable(), inc(3),
            self.x(), self.y(), -1, 1, -1, 1);
      attack_hitbox.damage(1);
      attack_hitbox.aoe(true);
      attack_hitbox.attack_strength(200);
      g.add_entity(@attack_hitbox.as_entity());
    }
    if (@attack_hitbox == null &&
        0 < self.heavy_intent() && self.heavy_intent() <= 10) {
      self.heavy_intent(11);
      @attack_hitbox = create_hitbox(@self.as_controllable(), inc(8),
            self.x(), self.y(), -1, 1, -1, 1);
      attack_hitbox.damage(3);
      attack_hitbox.aoe(true);
      attack_hitbox.attack_strength(600);
      g.add_entity(@attack_hitbox.as_entity());
    }
    can_dash();
    can_jump();
  }

  void state_dash() {
    int dir = state_timer == 0 ? self.x_intent() : 0;
    if (dir == 0) {
      dir = sgn(angular_momentum);
    }
    if (dir == -1) {
      angular_momentum = min(angular_momentum, -BLOB_DASH_ANGULAR_SPEED);
    } else if (dir == 1) {
      angular_momentum = max(angular_momentum, BLOB_DASH_ANGULAR_SPEED);
    }

    if (state_timer > inc(11.95)) {
      state = blob_state_roll;
      state_timer = 0;
    }
  }

  void state_jump() {
    if (state_timer > inc(11.95)) {
      state = blob_state_roll;
      state_timer = 0;
    }
  }

  bool can_dash() {
    if (self.dash_intent() == 1 ||
        self.fall_intent() == 1) {
      self.dash_intent(2);
      self.fall_intent(2);
      state = blob_state_dash;
      state_timer = 0;
      return true;
    }
    return false;
  }

  bool can_jump() {
    if (self.jump_intent() == 1) {
      self.jump_intent(2);
      state = blob_state_jump;
      state_timer = 0;
      return true;
    }
    return false;
  }

  float radius_multiplier() {
    if (state == blob_state_jump) {
      float per = state_timer / inc(4);
      if (per < 1) {
        return 1 + per;
      } else if (per < 2) {
        return 2;
      } else {
        return 2 - (per - 2) / 2.0;
      }
    }
    return 1;
  }

  float jump_force() {
    if (state == blob_state_jump) {
      float per = state_timer / inc(4);
      if (per < 1) {
        return s(BLOB_BASE_RADIUS) / inc(4);
      } else if (per < 2) {
        return 0;
      } else {
        return -s(BLOB_BASE_RADIUS) / inc(4) / 2;
      }
    }
    return 0;
  }

  float calc_radius() {
    return s(BLOB_BASE_RADIUS) * radius_multiplier();
  }

  void step() {
    float ff = self.freeze_frame_timer();
    if (ff > 0) {
      self.freeze_frame_timer(ff - inc(24));
      return;
    }

    float x = prev_x = self.x();
    float y = prev_y = self.y();
    float rotation = self.rotation();
    float x_speed = self.x_speed();
    float y_speed = self.y_speed();
    float radius = calc_radius();
    int yintent = self.y_intent();

    angular_momentum += inc(1000.0 * self.x_intent());
    angular_momentum = min(BLOB_MAX_ANGULAR_MOMENTUM, angular_momentum);
    angular_momentum = max(-BLOB_MAX_ANGULAR_MOMENTUM, angular_momentum);

    if (state == blob_state_roll) {
      state_roll();
    }
    if (state == blob_state_dash) {
      state_dash();
    } else if (state == blob_state_jump) {
      state_jump();
    }
    state_timer += inc(1.0);

    y_speed += s_inc(gravity);
    self.set_speed_xy(x_speed, y_speed);

    float tm = inc(1.0);
    for (int bounces = 0; bounces < BLOB_MAX_BOUNCES && tm > 1e-9; bounces++) {
      float speed = self.speed();
      float dir = self.direction();

      /* Clip our position backwards if needed. */
      for (int i = 0; i < BLOB_COLLISION_CHECKS; i++) {
        float edge_dir =
            dir + 180.0 * (i + 1) / (BLOB_COLLISION_CHECKS + 1) - 90;
        raycast@ rc = g.ray_cast_tiles(x, y,
            x + lengthdir_x(radius, edge_dir),
            y + lengthdir_y(radius, edge_dir));
        if (rc.hit()) {
          float dst = radius - distance(x, y, rc.hit_x(), rc.hit_y());
          x += lengthdir_x(dst, rc.angle());
          y += lengthdir_y(dst, rc.angle());
        }
      }

      float dx = lengthdir_x(tm * speed, dir);
      float dy = lengthdir_y(tm * speed, dir);

      bool found_collision = false;
      float collision_tm = tm;
      float collision_dir = 0;
      for (int i = 0; i < BLOB_COLLISION_CHECKS; i++) {
        float edge_dir =
            dir + 180.0 * (i + 1) / (BLOB_COLLISION_CHECKS + 1) - 90;

        float edge_x = x + lengthdir_x(radius, edge_dir);
        float edge_y = y + lengthdir_y(radius, edge_dir);
        raycast@ rc = g.ray_cast_tiles(edge_x, edge_y,
                                       edge_x + dx, edge_y + dy);
        if (rc.hit()) {
          float tm = distance(edge_x, edge_y, rc.hit_x(), rc.hit_y()) / speed;
          if  (tm < collision_tm) {
            found_collision = true;
            collision_tm = tm;
            collision_dir = rc.angle();

            int tx = rc.tile_x();
            int ty = rc.tile_y();
            tileinfo@ ti = g.get_tile(tx, ty, 19);
            if (ti.is_dustblock()) {
              ti.sprite_tile(0);
              g.set_tile(tx, ty, 19, @ti, true);
              clean_tiles.insertLast(tile_clean_data(tx, ty, 5.0));
            }
          }
        }
      }
      if (found_collision) {
        x += collision_tm * x_speed;
        y += collision_tm * y_speed;
        rotation += collision_tm * angular_momentum;

        /* Apply perpendicular force from collision. */
        float dx = lengthdir_x(1.0, collision_dir);
        float dy = lengthdir_y(1.0, collision_dir);
        float dt = x_speed * dx + y_speed * dy;

        float bounce_di = 0;
        if (dy < 0 && yintent == 1) {
          bounce_di = s(BLOB_BOUNCE_DI_FORCE);
        }
        if (abs(dt) < s(BLOB_STICK_SPEED_THRESH) + bounce_di) {
          x_speed -= dt * dx;
          y_speed -= dt * dy;
        } else {
          x_speed -= (1.0 + BLOB_BOUNCE_EFFICIENCY) * dt * dx;
          y_speed -= (1.0 + BLOB_BOUNCE_EFFICIENCY) * dt * dy;
          x_speed -= bounce_di * dx;
          y_speed -= bounce_di * dy;
        }

        float jf = jump_force();
        x_speed += jf * dx;
        y_speed += jf * dy;

        /* Apply parallel force from angular momentum. */
        dx = lengthdir_x(1.0, collision_dir + 90);
        dy = lengthdir_y(1.0, collision_dir + 90);
        dt = x_speed * dx + y_speed * dy;

        float e_init = BLOB_MOMENT_OF_INERTIA * sqr(angular_momentum) + sqr(dt);

        float speed_diff = degtorad(angular_momentum) * radius - dt;
        x_speed += speed_diff * BLOB_BOUNCE_ANGULAR_FRICTION * dx;
        y_speed += speed_diff * BLOB_BOUNCE_ANGULAR_FRICTION * dy;
        angular_momentum -= speed_diff * BLOB_BOUNCE_ANGULAR_FRICTION;
        
        tm -= collision_tm;
        self.set_speed_xy(x_speed, y_speed);
        angular_momentum = min(BLOB_MAX_ANGULAR_MOMENTUM, angular_momentum);
        angular_momentum = max(-BLOB_MAX_ANGULAR_MOMENTUM, angular_momentum);

        g.project_tile_filth(x, y, 1, 1, 0, 0, 3 * radius, 360,
                             true, true, true, true, false, true);
      } else {
        x += tm * x_speed;
        y += tm * y_speed;
        rotation += collision_tm * angular_momentum;
        break;
      }
    }

    while (rotation < -180) rotation += 360;
    while (rotation > 180) rotation -= 360;

    self.set_xy(x, y);

    float fric = pow(BLOB_AIR_FRICTION, inc(1));
    x_speed *= fric;
    y_speed *= fric;

    float ang_diff = angular_momentum;
    angular_momentum *= pow(BLOB_AIR_FRICTION, inc(1));
    ang_diff = angular_momentum - ang_diff;

    float air_force = ang_diff * BLOB_AIR_FRICTION_DI_COEFF;
    float air_force_dir = point_angle(0, 0, x_speed, y_speed) - 90;
    x_speed += lengthdir_x(air_force, air_force_dir);
    y_speed += lengthdir_y(air_force, air_force_dir);
    self.set_speed_xy(x_speed, y_speed);

    self.rotation(rotation);
    self.hit_rectangle(-radius, radius, -radius, radius);
    self.base_rectangle(-radius, radius, -radius, radius);
    player_collision.rectangle(y - radius, y + radius, x - radius, x + radius);

    if (@attack_hitbox != null) {
      if (attack_hitbox.triggered()) {
        @attack_hitbox = null;
      } else {
        attack_hitbox.x(x);
        attack_hitbox.y(y);
        attack_hitbox.base_rectangle(-radius * 2, radius * 2,
                                     -radius * 2, radius * 2);
      }
    }

    for (int i = 0; i < clean_tiles.size(); i++) {
      clean_tiles[i].timer -= inc(24);
      if (clean_tiles[i].timer <= 0) {
        g.set_tile(clean_tiles[i].x, clean_tiles[i].y, 19, false, 0, 0, 0, 0);
        clean_tiles[i] = clean_tiles[clean_tiles.size() - 1];
        clean_tiles.resize(clean_tiles.size() -1 );
        i--;
      }
    }
  }

  void editor_step() {
  }

  void draw(float subframe) {
    float x = lerp(prev_x, self.x(), subframe);
    float y = lerp(prev_y, self.y(), subframe);
    float scale = self.scale();
    float rotation = self.rotation();
    float radius = calc_radius();

    int colour = 0xFFFFFFFF;
    if (@attack_hitbox != null) {
      float per = attack_hitbox.state_timer() /
                  max(0.001, attack_hitbox.activate_time());
      int part = int(255 * (1.0 - per));
      colour = 0xFF000000 | (part << 16) | (part << 8) | part;
    }
    spr.draw(self.layer(), 15, x, y, rotation, scale * radius_multiplier(),
             colour);

    /*
    g.draw_rectangle_world(self.layer(), 9, x - radius, y - radius, x + radius,
                     y + radius, 0, 0xFFFFFFFF);
    */
  }

  void editor_draw(float subframe) {
    draw(1.0);
  }

  void break_combo() {
    bool found = false;
    for (int i = 0; i < num_cameras(); i++) {
      if (self.is_same(controller_entity(i))) {
        found = true;
        break;
      }
    }
    if (found) {
      g.combo_break_count(g.combo_break_count() + 1);
    }
  }
}
