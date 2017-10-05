const float DEG_TO_RAD = 1.0 / 180.0 * 3.14159265358979;

class simple_transform {
  float x;
  float y;
  float rot;
  float scale;

  simple_transform() {
    x = 0;
    y = 0;
    rot = 0;
    scale = 1;
  }

  simple_transform(float _x, float _y, float _rot, float _scale) {
    x = _x;
    y = _y;
    rot = _rot;
    scale = _scale;
  }
}

class sprite_rectangle {
  float top;
  float bottom;
  float left;
  float right;

  sprite_rectangle() {
    top = 0;
    bottom = 0;
    left = 0;
    right = 0;
  }
}

class sprite_group {
  sprites@ spr;

  array<string> sprite_names;
  array<simple_transform> sprite_transforms;

  sprite_group() {
    @spr = create_sprites();
  }

  void add_sprite(string sprite_set, string sprite_name,
                  int align_x = 0, int align_y = 0,
                  float off_x = 0, float off_y = 0, float rot = 0,
                  float scale = 1) { 
    spr.add_sprite_set(sprite_set);
    sprite_names.insertLast(sprite_name);

    rectangle@ rc = spr.get_sprite_rect(sprite_name, 0);

    float width = rc.get_width();
    float height = rc.get_height();

    float rx = (rc.left() + width * (align_x + 1) / 2.0) * scale;
    float ry = (rc.top() + height * (align_y + 1) / 2.0) * scale;
    float cs = cos(rot * DEG_TO_RAD);
    float sn = sin(rot * DEG_TO_RAD);
    off_x -= rx * cs - ry * sn;
    off_y -= ry * cs + rx * sn;

    sprite_transforms.insertLast(
          simple_transform(off_x, off_y, rot, scale));
  }

  void draw(int layer, int sub_layer, float x, float y, float rot, float scale,
            int colour = 0xFFFFFFFF) {
    float cs = cos(rot * DEG_TO_RAD);
    float sn = sin(rot * DEG_TO_RAD);
    for (int i = 0; i < sprite_names.size(); i++) {
      simple_transform tx = sprite_transforms[i];

      float px = tx.x * cs - tx.y * sn;
      float py = tx.y * cs + tx.x * sn;
      spr.draw_world(layer, sub_layer, sprite_names[i], 0, 1,
                     x + px * scale, y + py * scale,
                     rot + tx.rot, tx.scale * scale, tx.scale * scale, colour);
    }
  }

  sprite_rectangle get_rectangle(float rot, float scale) {
    float cs = cos(rot * DEG_TO_RAD);
    float sn = sin(rot * DEG_TO_RAD);

    sprite_rectangle rc;
    for (int i = 0; i < sprite_names.size(); i++) {
      simple_transform tx = sprite_transforms[i];

      float px = tx.x * cs - tx.y * sn;
      float py = tx.y * cs + tx.x * sn;
      rectangle@ src = spr.get_sprite_rect(sprite_names[i], 0);

      float scs = cos((rot + tx.rot) * DEG_TO_RAD);
      float ssn = sin((rot + tx.rot) * DEG_TO_RAD);
      array<float> xs = {src.left(), src.right()};
      array<float> ys = {src.top(), src.bottom()};
      for (int j = 0; j < 2; j++) {
        for (int k = 0; k < 2; k++) {
          float cx = (px + (xs[j] * scs - ys[k] * ssn) * tx.scale) * scale;
          float cy = (py + (ys[k] * scs + xs[j] * ssn) * tx.scale) * scale;

          rc.left = min(rc.left, cx);
          rc.right = max(rc.right, cx);
          rc.top = min(rc.top, cy);
          rc.bottom = max(rc.bottom, cy);
        }
      }
    }
    return rc;
  }
}

