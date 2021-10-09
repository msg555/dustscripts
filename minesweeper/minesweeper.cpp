#include "replay_rand.cpp"

/* Mouse state masks from the API */
const int LEFT_CLICK = 0x4;
const int RIGHT_CLICK = 0x8;
const int MIDDLE_CLICK = 0x10;

/* Utility arrays to help enumerate neighbors of a cell */
const array<int> dr = {-1, 0, 1, 0, -1, -1, 1, 1};
const array<int> dc = {0, -1, 0, 1, -1, 1, -1, 1};

class script {
  /* Driver script object making grid parameters/display parameters
   * configurable.
   */
  [int] int rows;
  [int] int cols;
  [int] int bombs;
  [int] float tile_size;

  /* Utility object that manages the rand() state so the rand stream can be
   * replayed. */
  replay_rand rrnd;

  script() {
    /* Initialize to expert settings */
    rows = 16;
    cols = 30;
    bombs = 99;
    tile_size = 48.0;
  }

  void step(int) {
    rrnd.step();
  }

  void spawn_player(message@ msg) {
    /* Spawn the player entity as the minesweeper controllable */
    scriptenemy@ ent = create_scriptenemy(
      minesweeper(rows, cols, bombs, tile_size)
    );
    ent.x(msg.get_float("x"));
    ent.y(msg.get_float("y"));
    msg.set_entity("player", @ent.as_entity());
  }
}

/* Tracks the state of each cell */
class cell {
  bool bomb; // Is the cell a mine or not
  bool killed; // If a mine, did this mine kill the player?
  bool revealed; // Are the contents of this cell visible?
  bool marked; // Has the user "marked" this cell with a flag?
  int bomb_count; // Number of adjacent mines to this cell

  cell() {
  }
}

// Just eyeballed these, not an exact match to anything.
const uint COLOR_UNPRESSED_CELL = 0xFF777777;
const uint COLOR_UNPRESSED_TOP = 0xFFDDDDDD;
const uint COLOR_UNPRESSED_LFT = 0xFFDDDDDD;
const uint COLOR_UNPRESSED_BOT = 0xFF666666;
const uint COLOR_UNPRESSED_RHT = 0xFF666666;
const uint COLOR_PRESSED_CELL = 0xFF666666;
const uint COLOR_PRESSED_BORDER = 0xFF333333;
const uint COLOR_REVEALED_CELL = 0xFF555555;
const uint COLOR_REVEALED_BORDER = 0xFF111111;

// Pulled these colors out of a screenshot of the classic game.
const array<uint> COLOR_COUNTS = {
  0,
  0xFF0100FE, // 1
  0xFF017F01, // 2
  0xFFFE0000, // 3
  0xFF010080, // 4
  0xFF810102, // 5
  0xFF008081, // 6
  0xFF000000, // 7
  0xFF808080, // 8
};

class single_sprite {
  /* Container class that manages sprites object and drawing of a single sprite
   * frame. Performs measurements so that the sprite can be drawn centered and
   * of a specified radius into a passed canvas.
   */
  sprites@ spr;

  string sprite_name;
  int frame;
  int palette;

  float top, lft, bot, rht;

  single_sprite(string sprite_set, string sprite_name, int frame, int palette) {
    @spr = @create_sprites();
    spr.add_sprite_set(sprite_set);

    this.sprite_name = sprite_name;
    this.frame = frame;
    this.palette = palette;

    rectangle@ rec = spr.get_sprite_rect(sprite_name, frame);
    top = rec.top();
    lft = rec.left();
    bot = rec.bottom();
    rht = rec.right();
  }

  void draw(canvas@ cvs, float cx, float cy, float radius, float rotation=0, int colour=0xFFFFFFFF) {
    float actual_scale = max(rht - lft, bot - top);
    float scale_factor = radius / actual_scale * 2;
    cvs.draw_sprite(
      @spr, sprite_name, frame, palette,
      cx - (lft + rht) / 2 * scale_factor, cy - (top + bot) / 2 * scale_factor,
      rotation, scale_factor, scale_factor, colour
    );
  }
}

class minesweeper : enemy_base {
  /* Minesweeper controllable */
  int rows;
  int cols;
  int bombs;
  int marks; /* count of number of marked cells */
  float tile_size;

  bool grid_ready;
  array<array<cell> > grid;

  int reveal_count; /* Numver of cells that have been revealed */
  bool dead; /* Set when the player reveals a mine */
  bool finished; /* Set when the player reveals all non-mines */
  bool show_mouse;

  scene@ g;
  scriptenemy@ self;
  canvas@ cvs;
  textfield@ txt;

  single_sprite@ flag;
  single_sprite@ bomb;

  int lock_out_timer; /* Timer at the start after spawning locking out inputs */
  int last_mouse_st; /* The mouse state from the last time step() was called */
  int last_r; /* (last_r, last_c) give the coordinates of the mouse last step */
  int last_c;

  minesweeper(int rows, int cols, int bombs, float tile_size) {
    this.rows = rows;
    this.cols = cols;
    this.bombs = bombs;
    this.tile_size = tile_size;
    @g = @get_scene();
  }

  void make_grid(int avoid_r, int avoid_c) {
    /* Assigns mines to cells and calculates the bomb_count cell metadata.
     * This is the only method that uses rand() and happens when the user
     * clicks on the first cell.
     */
    array<int> coords;
    for (int r = 0; r < rows; r++) {
      for (int c = 0; c < cols; c++) {
        if (r != avoid_r || c != avoid_c) {
          coords.insertLast(r * cols + c);
        }
      }
    }
    for (int i = 0; i < bombs && i < int(coords.size()); i++) {
      int j = i + rand() % (coords.size() - i);
      int ind = coords[j];
      coords[j] = coords[i];
      grid[ind / cols][ind % cols].bomb = true;
    }

    for (int r = 0; r < rows; r++) {
      for (int c = 0; c < cols; c++) {
        for (int i = 0; i < 8; i++) {
          int nr = r + dr[i];
          int nc = c + dc[i];
          if (nr < 0 || nr >= rows || nc < 0 || nc >= cols) {
            continue;
          }
          if (grid[nr][nc].bomb) {
            grid[r][c].bomb_count++;
          }
        }
      }
    }
    grid_ready = true;
  }

  void init(script@, scriptenemy@ self) {
    @this.self = @self;
    @cvs = create_canvas(false, self.layer(), 1);
    @txt = create_textfield();
    @flag = @single_sprite("flag", "cidle", 1, 1);
    @bomb = @single_sprite("editor", "skull", 0, 1);

    txt.set_font("Caracteres", 36);
    txt.align_horizontal(0);
    txt.align_vertical(0);

    grid_ready = false;
    grid.resize(0);
    grid.resize(rows);
    for (int r = 0; r < rows; r++) {
      grid[r].resize(cols);
    }
    lock_out_timer = 55;
  }

  void step() {
    if (dead) {
      return;
    }
    if (!finished && reveal_count + bombs == rows * cols) {
      finished = true;
      g.end_level(self.x(), self.y());
    }
    if (finished) {
      return;
    }
    if (lock_out_timer > 0) {
      lock_out_timer--;
      return;
    }

    int player = self.player_index();
    if (player == -1) {
      last_mouse_st = 0;
      return;
    }

    /* Figure out what cell the mouse is over and what the mouse state is. */
    float ent_x = self.x();
    float ent_y = self.y();
    float lft = ent_x - cols / 2.0 * tile_size;
    float top = ent_y - rows / 2.0 * tile_size;

    int layer = self.layer();
    float x = g.mouse_x_world(player, layer);
    float y = g.mouse_y_world(player, layer);

    int mst = g.mouse_state(player);

    last_r = int(floor((y - top) / tile_size));
    last_c = int(floor((x - lft) / tile_size));
    if (last_r < 0 || last_r >= rows || last_c < 0 || last_c >= cols) {
      last_mouse_st = mst;
      return;
    }

    cell@ c = @grid[last_r][last_c];
    if ((mst & RIGHT_CLICK) != 0 && (last_mouse_st & RIGHT_CLICK) == 0) {
      // pos-edge right click
      if (!c.revealed) {
        c.marked = !c.marked;
        marks += c.marked ? 1 : -1;
      }
    }

    array<int> reveal;
    bool left_click = (mst & LEFT_CLICK) == 0 && (last_mouse_st & LEFT_CLICK) != 0;
    bool middle_click = (mst & MIDDLE_CLICK) == 0 && (last_mouse_st & MIDDLE_CLICK) != 0;
    if (!c.marked && (left_click || middle_click)) {
      /* Figure out what set of cells to should be revealed based on the click.
       * left/middle clicking a revealed cell should reveal all its neighbors.
       */
      if (c.revealed) {
        int cnt = 0;
        for (int i = 0; i < 8; i++) {
          int nr = last_r + dr[i];
          int nc = last_c + dc[i];
          if (nr < 0 || nr >= rows || nc < 0 || nc >= cols) {
            continue;
          }
          if (grid[nr][nc].marked) {
            cnt++;
          } else {
            reveal.insertLast(nr * cols + nc);
          }
        }
        if (cnt != c.bomb_count) {
          reveal.resize(0);
        }
      } else if (left_click) {
        /* left clicking an unrevealed cell should reveal just that cell */
        reveal.insertLast(last_r * cols + last_c);
      }
    }

    /* Create the grid if this is the first click */
    if (reveal.size() != 0 && !grid_ready) {
      make_grid(last_r, last_c);
    }

    /* Reveal all cells in the reveal array. If one or more of those cells
     * has no bomb neighbors automatically expand our search into its
     * neighbors in BFS fashion.
     */
    for (uint i = 0; i < reveal.size(); i++) {
      int ind = reveal[i];
      int row = ind / cols;
      int col = ind % cols;
      @c = @grid[row][col];
      if (c.revealed) {
        continue;
      }
      c.revealed = true;
      reveal_count++;
      if (c.bomb) {
        dead = true;
        c.killed = true;
      }
      if (c.bomb_count == 0) {
        for (int j = 0; j < 8; j++) {
          int nr = row + dr[j];
          int nc = col + dc[j];
          if (nr < 0 || nr >= rows || nc < 0 || nc >= cols) {
            continue;
          }
          reveal.insertLast(nr * cols + nc);
        }
      }
    }

    last_mouse_st = mst;

    if (dead) {
      /* Can uncomment if you want the player to die when they reveal a mine. I
       * optted to just lock the user out of input so they can continue to look
       * at the board state.
       */
      // g.load_checkpoint();
    }
  }
  
  void draw(float) {
    float ent_x = self.x();
    float ent_y = self.y();
    float lft = ent_x - cols / 2.0 * tile_size;
    float top = ent_y - rows / 2.0 * tile_size;

    cvs.reset();
    cvs.layer(self.layer());
    cvs.multiply(tile_size, 0, 0, tile_size, lft, top);

    txt.text("" + (bombs - marks));
    txt.colour(0xFFFFFFFF);
    float txt_scale = 0.8 / 36.0;
    cvs.draw_text(@txt, 1, -1, txt_scale, txt_scale, 0);

    bool on_revealed_cell = 0 <= last_r && last_r < rows &&
        0 <= last_c && last_c < cols && grid[last_r][last_c].revealed;

    bool pressing = (last_mouse_st & LEFT_CLICK) != 0 || (
      on_revealed_cell && (last_mouse_st & MIDDLE_CLICK) != 0
    );
    for (int i = 0; i < rows; i++) {
      for (int j = 0; j < cols; j++) {
        cell@ c = @grid[i][j];
        bool pressed = false;
        if (pressing) {
          if (i == last_r && j == last_c) {
            pressed = true;
          } else if (on_revealed_cell && abs(i - last_r) <= 1 && abs(j - last_c) <= 1) {
            pressed = true;
          }
        }
        bool revealed = c.revealed || dead;

        cvs.push();
        cvs.translate(j, i);

        draw_bg(revealed, pressed);
        if (!revealed && c.marked) {
          draw_flag();
        }
        if (revealed) {
          if (c.bomb) {
            draw_bomb(c.killed);
          } else {
            draw_count(c.bomb_count);
          }
        }

        cvs.pop();
      }
    }

    if (is_replay()) {
      int player = self.player_index();
      if (player != -1) {
        int layer = self.layer();
        float x = g.mouse_x_world(player, layer);
        float y = g.mouse_y_world(player, layer);
        g.draw_rectangle_world(layer, 10, x - 5, y - 5, x + 5, y + 5, 0, 0xFFFF0000);
      }
    }
  }

  void draw_flag() {
    cvs.sub_layer(1);
    flag.draw(@cvs, 0.5, 0.5, 0.35);
  }

  void draw_bomb(bool killed) {
    cvs.sub_layer(1);
    bomb.draw(@cvs, 0.5, 0.5, 0.35, 0, killed ? 0xFFFF7777 : 0xFFFFFFFF);
  }

  void draw_count(int count) {
    if (count == 0) {
      return;
    }
    cvs.sub_layer(1 + count);
    txt.text("" + count);
    txt.colour(COLOR_COUNTS[count]);
    float txt_scale = 0.8 / 36.0;
    cvs.draw_text(@txt, 0.5, 0.5, txt_scale, txt_scale, 0);
  }

  void draw_bg(bool revealed, bool pressed) {
    cvs.sub_layer(1);
    cvs.draw_rectangle(
      0, 0, 1, 1,
      0, revealed ? COLOR_REVEALED_CELL : pressed ? COLOR_PRESSED_CELL : COLOR_UNPRESSED_CELL
    );
    if (revealed) {
      draw_sides(0.02, COLOR_REVEALED_BORDER, COLOR_REVEALED_BORDER,
                 COLOR_REVEALED_BORDER, COLOR_REVEALED_BORDER);
    } else if (pressed) {
      draw_sides(0.02, COLOR_PRESSED_BORDER, COLOR_PRESSED_BORDER,
                 COLOR_PRESSED_BORDER, COLOR_PRESSED_BORDER);
    } else {
      draw_sides(0.1, COLOR_UNPRESSED_TOP, COLOR_UNPRESSED_LFT,
                 COLOR_UNPRESSED_BOT, COLOR_UNPRESSED_RHT);
    }
  }

  void draw_sides(float margin, uint c_top, uint c_lft, uint c_bot, uint c_rht) {
    cvs.draw_quad(
      false, 0, 0, 1, 0, 1 - margin, margin, margin, margin,
      c_top, c_top, c_top, c_top
    );
    cvs.draw_quad(
      false, 0, 0, margin, margin, margin, 1 - margin, 0, 1,
      c_lft, c_lft, c_lft, c_lft
    );
    cvs.draw_quad(
      false, 0, 1, 1, 1, 1 - margin, 1 - margin, margin, 1 - margin,
      c_bot, c_bot, c_bot, c_bot
    );
    cvs.draw_quad(
      false, 1, 0, 1, 1, 1 - margin, 1 - margin, 1 - margin, margin,
      c_rht, c_rht, c_rht, c_rht
    );
  }
}
