const uint NUM_ENCODERS = 4;

/* Usage:
 *
 * Instantiate a single instance of replay_rand in your script. Call step() on
 * the replay_rand object every time script.step is called. 
 *
 * After frame 21 srand will have been called with a seed that will be
 * reproduced in a replay. Ensure that you do not rely on randomness before
 * frame 20 (this is normally before the player even has control).
 *
 * This can potentially fail if there is no player entity or if the player
 * entity moves significantly in the first 20 frames (dustman free fall should
 * be fine but entering a zip would probably break things).
 */
class replay_rand {
  scene@ g;
	array<uint> encoders;
  uint seed;

  int ecx;
  int ecy;
  uint frame_counter;

  replay_rand() {
    @g = @get_scene();
  }

  void step() {
    if (encoders.size() == 0) {
      // Add some "encoder" entities at determinisitc positions relative
      // to the plyaer entity.
      init_encoders();
      return;
    }
    if (frame_counter == 10) {
      /* For non-replays generate a target seed and move the entities
       * to encode that seed. */
      encode_seed();
    }
    if (frame_counter == 20) {
      /* Decode frames happen every 8 frames if there is significant movement
       * near the player. Therefore 10 frames later should be more than nough
       * to recover the encoded seed.
       */
      decode_seed();
    }
    ++frame_counter;
  }

  bool seed_set() {
    return frame_counter >= 20;
  }

  void init_encoders() {
    controllable@ ec = @controller_controllable(0);
    if (@ec == null) {
      return;
    }

    ecx = int(ec.x());
    ecy = int(ec.y());
    for (uint i = 0; i < NUM_ENCODERS; i++) {
      scriptenemy@ ent = create_scriptenemy(replay_rand_dummy());
      ent.x(ecx);
      ent.y(ecy);
      g.add_entity(@ent.as_entity());
      encoders.insertLast(ent.id());
    }
  }

  void encode_seed() {
    if (is_replay()) {
      return;
    }

    srand(timestamp_now() + get_time_us());
    seed = rand();
    puts("seed is " + seed);
    for (uint i = 0; i < NUM_ENCODERS; i++) {
      entity@ ent = @entity_by_id(encoders[i]);
      if (@ent == null) {
        puts("encoding seed failed");
        return;
      }
      ent.x(ecx - 128 + ((seed >> (i * 8)) & 0xFF));
      ent.y(ecy + 100);
    }
  }

  void decode_seed() {
    seed = 0;
    for (uint i = 0; i < NUM_ENCODERS; i++) {
      entity@ ent = @entity_by_id(encoders[i]);
      if (@ent == null) {
        puts("seed reconstruction failed");
        return;
      }
      int enc_x = int(round(ent.x())) + 128 - ecx;
      int enc_y = int(round(ent.y()));
      if (enc_y == ecy) {
        puts("desync frames missing, seed reconstruction failed");
        return;
      }
      seed |= enc_x << (i * 8);
    }
    puts("constructed seed " + seed);
    srand(seed);
  }
}

class replay_rand_dummy : enemy_base {
}
