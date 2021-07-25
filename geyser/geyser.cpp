const int GEYSER_STATE_INACTIVE = 0;
const int GEYSER_STATE_ACTIVE = 1;

float cos_deg(int deg) {
  return sin_deg(deg + 90);
}

float sin_deg(int deg) {
  deg %= 360;
  if (deg <= -180) deg += 360;
  if (deg > 180) deg -= 360;

  float m = 1;
  if (deg < 0) {
    m = -1;
    deg = -deg;
  }
  if (90 < deg) {
    deg = 180 - deg;
  }

  if (deg == 0) return 0;
  if (deg == 90) return m * 1;
  return m * sin(deg / 180.0 * 3.14159265358979);
}

class geyser : trigger_base {
  scene@ g;
  script@ s;
  scripttrigger@ self;

  [angle] int rotation;
  [text] int width;
  [text] int depth;
  [text] float activation_time;
  [text] float cooldown_time;
  [text] int emitter_id;
  [text] int emitter_layer;
  [text] float max_lift;
  [check] bool lift_off_ground;
  [check] bool ignore_visibility;

  int state;
  float state_timer;

  entity@ emitter;

  float mnx, mxx, mny, mxy;

  geyser() {
    rotation = 0;
    width = 96;
    depth = 480;
    activation_time = 2;
    cooldown_time = 1;
    emitter_id = 41;
    emitter_layer = 19;
    max_lift = 5000;
    lift_off_ground = false;
    ignore_visibility = false;

    state = GEYSER_STATE_INACTIVE;
    cooldown_time = 0;
  }

  void init(script@ s, scripttrigger@ self) {
    @this.g = @get_scene();
    @this.s = s;
    @this.self = self;
  }

  void update_bounds(float cx, float cy) {
    array<float> rx;
    array<float> ry;
    rx.insertLast(-width / 2.0); ry.insertLast(0);
    rx.insertLast(width / 2.0); ry.insertLast(0);
    rx.insertLast(width / 2.0); ry.insertLast(-depth);
    rx.insertLast(-width / 2.0); ry.insertLast(-depth);

    mnx = mxx = cx;
    mny = mxy = cy;

    float cosm = cos_deg(rotation);
    float sinm = sin_deg(rotation);
    for (uint i = 0; i < 4; i++) {
      float x = rx[i];
      float y = ry[i];
      rx[i] = cx + x * cosm - y * sinm;
      ry[i] = cy + y * cosm + x * sinm;
      mnx = min(mnx, rx[i]);
      mxx = max(mxx, rx[i]);
      mny = min(mny, ry[i]);
      mxy = max(mxy, ry[i]);
    }
  }

  void editor_draw(float) {
    canvas@ cvs = @create_canvas(false, 18, 10);
    cvs.translate(self.x(), self.y());
    cvs.rotate(rotation, 0, 0);
    cvs.draw_rectangle(-width / 2.0, -depth, width / 2.0, 0, 0, 0x3300FF00);
  }

  void step() {
    float inc_val = self.time_warp() / 60.0;

    state_timer -= inc_val;
    if (state_timer < 1e-9) {
      state_timer = 0;
    }
    if (state == GEYSER_STATE_ACTIVE && state_timer <= 0) {
      state = GEYSER_STATE_INACTIVE;
      state_timer = cooldown_time;
      if (@emitter != null) {
        g.remove_entity(@emitter);
      }
    }
    if (state == GEYSER_STATE_INACTIVE && state_timer > 0) {
      return;
    }

    float cx = self.x();
    float cy = self.y();
    update_bounds(cx, cy);

    array<int> col_types;
    col_types.insertLast(1);
    col_types.insertLast(5);
    array<controllable@> col_entities;
    for (uint i = 0; i < col_types.size(); i++) {
      int nc = g.get_entity_collision(mny, mxy, mnx, mxx, col_types[i]);
      for (int j = 0; j < nc; j++) {
        col_entities.insertLast(@g.get_entity_collision_index(j).as_controllable());
      }
    }

    float normx = cos_deg(rotation - 90);
    float normy = sin_deg(rotation - 90);
    for (uint i = 0; i < col_entities.size(); i++) {
      controllable@ e = @col_entities[i];
      if (@e == null) {
        continue;
      }

      float ex = e.x();
      float ey = e.y();

      // Check if entity too far (or in front of) geyser
      float dp = (ex - cx) * normx + (ey - cy) * normy;
      if (dp < -24 || depth < dp) {
        continue;
      }
      float lift = inc_val * max_lift * max(0.0, min(1.0, 1.0 - dp / depth));

      // Check if entity too left/right of geyser
      dp = (ex - cx) * cos_deg(rotation) + (ey - cy) * sin_deg(rotation);
      if (dp < -width / 2.0 || width / 2.0 < dp) {
        continue;
      }

      // Check if the entity is visible through tiles
      if (!is_entity_visible(cx, cy, ex, ey)) {
        continue;
      }

      if (state == GEYSER_STATE_INACTIVE) {
        start_geyser();
      }

      float x_speed = e.x_speed();
      float y_speed = e.y_speed();
      if (!lift_off_ground && e.ground()) {
        int ground_ang = e.ground_surface_angle();
        float ground_x = cos_deg(ground_ang);
        float ground_y = sin_deg(ground_ang);
        float norm = lift * (ground_x * normx + ground_y * normy);
        e.set_speed_xy(x_speed + ground_x * norm, y_speed + ground_y * norm);
      } else {
        e.set_speed_xy(x_speed + lift * normx, y_speed + lift * normy);
      }
    }
  }

  bool is_entity_visible(float cx, float cy, float ex, float ey) {
    if (ignore_visibility) {
      return true;
    }
    float dirx = ex - cx;
    float diry = ey - cy;
    float dirn = max(1, sqrt(dirx * dirx + diry * diry));

    raycast@ rc = g.ray_cast_tiles(cx + dirx/dirn * 24.0, cy + diry/dirn * 24.0, ex, ey);
    if (rc.hit()) {
      return false;
    }

    @rc = g.ray_cast_tiles(ex, ey, cx + dirx/dirn * 24.0, cy + diry/dirn * 24.0);
    return !rc.hit();
  }

/*
  void activate(controllable@ e) {
    if (state != GEYSER_STATE_INACTIVE || state_timer > 0) {
      return;
    }
    if (!is_entity_visible(self.x(), self.y(), e.x(), e.y())) {
      return;
    }
    start_geyser();
  }
*/

  void start_geyser() {
    state = GEYSER_STATE_ACTIVE;
    state_timer = activation_time;

    @emitter = create_entity("entity_emitter");
    emitter.layer(emitter_layer);
    emitter.set_xy(self.x(), self.y());

    varstruct@ vars = emitter.vars();
    vars.get_var("e_rotation").set_int32(rotation);
    vars.get_var("width").set_int32(width);
    vars.get_var("height").set_int32(50);
    vars.get_var("emitter_id").set_int32(emitter_id);
    g.add_entity(@emitter, false);
  }
}
