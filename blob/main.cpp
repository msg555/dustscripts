#include "blob.cpp"

class script {
  scene@ g;

  script() {
    @g = get_scene();
  }

  void spawn_player(message@ msg) {
    float x = msg.get_float("x");
    float y = msg.get_float("y");

    blob@ b = blob();
    scriptenemy@ pl = create_scriptenemy(@b);
    y -= b.calc_radius();
    pl.x(x);
    pl.y(y);
    b.prev_x = x;
    b.prev_y = y;

    msg.set_entity("player", pl.as_entity());
  }

  void entity_on_remove(entity@ e) {
    for (int i = 0; i < num_cameras(); i++) {
      if (controller_controllable(i).is_same(@e)) {
        if (num_cameras() > 1) {
          scriptenemy@ se = create_scriptenemy(blob_respawner(i, 1.0));
          se.x(g.get_checkpoint_x(i));
          se.y(g.get_checkpoint_y(i));
          controller_entity(i, @se.as_controllable());
          g.add_entity(@se.as_entity(), false);
        } else {
          g.combo_break_count(g.combo_break_count() + 1);
          g.load_checkpoint();
        }
      }
    }
  }
}

class blob_respawner : enemy_base {
  scene@ g;
  scriptenemy@ self;

  int player;
  float timer;

  blob_respawner(int player, float timer) {
    @g = get_scene();

    this.player = player;
    this.timer = timer;
  }

  void init(script@, scriptenemy@ self) {
    @this.self = @self;
  }

  void step() {
    if (timer < 0) {
      return;
    }
    timer -= inc(1);
    if (timer < 0) {
      blob@ b = blob();
      scriptenemy@ se = create_scriptenemy(@b);
      float x = self.x();
      float y = self.y() - b.calc_radius();
      se.x(x);
      se.y(y);
      b.prev_x = x;
      b.prev_y = y;
      g.add_entity(@se.as_entity(), false);
      controller_entity(player, @se.as_controllable());

      g.remove_entity(@self.as_entity());
    }
  }

  float inc(float x) {
    return x / 60.0 * self.time_warp();
  }
}
