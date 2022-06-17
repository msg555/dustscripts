const uint NUM_ENCODERS = 4;

/* Usage:
 *
 * Instantiate a single instance of value_recorder in your script. Call step() on
 * the value_recorder object every time script.step is called. 
 *
 * After frame 21 value_recorder.state should be RecorderState.DONE if the
 * value was successfully written/recovered. You can access the value with
 * value_recorder.value. If there was an error and the value could not be
 * written or recorded the state will be RecorderState.FAILED instead.
 *
 * This can potentially fail if there is no player entity or if the player
 * entity moves significantly in the first 20 frames (dustman free fall should
 * be fine but entering a zip would probably break things).
 */

enum RecorderState {
  INIT = 0,
  ENCODED = 1,
  DONE = 2,
  FAILED = 3
};

class ValueRecorder {
  scene@ g;
	array<uint> encoders;

  int value;
  RecorderState state;

  int ecx;
  int ecy;
  uint frame_counter;

  ValueRecorder(int value) {
    @g = @get_scene();
    state = RecorderState::INIT;
    this.value = value;
  }

  void step() {
    if (encoders.size() == 0) {
      // Add some "encoder" entities at determinisitc positions relative
      // to the plyaer entity.
      if (!init_encoders()) {
        state = RecorderState::FAILED;
      }
      return;
    }
    if (state == RecorderState::INIT && frame_counter == 10) {
      /* For non-replays move the entities to encode the value. */
      if (!encode_value()) {
        state = RecorderState::FAILED;
      } else {
        state = RecorderState::ENCODED;
      }
    }
    if (state == RecorderState::ENCODED && frame_counter == 20) {
      /* Decode frames happen every 8 frames if there is significant movement
       * near the player. Therefore 10 frames later should be more than nough
       * to recover the encoded value.
       */
      if (!decode_value()) {
        state = RecorderState::FAILED;
      } else {
        state = RecorderState::DONE;
      }
    }
    ++frame_counter;
  }

  bool init_encoders() {
    controllable@ ec = @controller_controllable(0);
    if (@ec == null) {
      puts("no entity attached to controller 0, encode failed");
      return false;
    }

    ecx = int(ec.x());
    ecy = int(ec.y());
    for (uint i = 0; i < NUM_ENCODERS; i++) {
      scriptenemy@ ent = create_scriptenemy(replay_value_dummy());
      puts("CREATING " + ecx + ", " + ecy);
      ent.x(ecx);
      ent.y(ecy);
      g.add_entity(@ent.as_entity());
      encoders.insertLast(ent.id());
    }
    return true;
  }

  bool encode_value() {
    if (is_replay()) {
      return true;
    }

    uint val = uint(value);
    for (uint i = 0; i < NUM_ENCODERS; i++) {
      entity@ ent = @entity_by_id(encoders[i]);
      if (@ent == null) {
        puts("entity missing, encoding value failed");
        return false;
      }
      ent.x(ecx - 128 + ((val >> (i * 8)) & 0xFF));
      ent.y(ecy + 100);
    }
    return true;
  }

  bool decode_value() {
    uint val = 0;
    for (uint i = 0; i < NUM_ENCODERS; i++) {
      entity@ ent = @entity_by_id(encoders[i]);
      if (@ent == null) {
        puts("entity missing, value reconstruction failed");
        return false;
      }
      int enc_x = int(round(ent.x())) + 128 - ecx;
      int enc_y = int(round(ent.y()));
      puts("" + enc_y + ", " + ecy + ", " + enc_x + ", " + ecx);
      if (enc_y == ecy) {
        puts("desync frames missing, value reconstruction failed");
        return false;
      }
      val |= enc_x << (i * 8);
    }

    value = int(val);
    puts("value recovered " + value);
    return true;
  }
}

class replay_value_dummy : enemy_base {
  void init(script@, scriptenemy@ self) {
    self.auto_physics(false);
  }
}
