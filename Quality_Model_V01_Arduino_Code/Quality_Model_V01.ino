#include <Quality_model_inferencing.h>
#include <Arduino_BMI270_BMM150.h>

#define CONVERT_G_TO_MS2    9.80665f
#define MAX_ACCEPTED_RANGE  2.0f
#define SAMPLE_FREQ_HZ      59
#define COOLDOWN_MS         2000

// ─── Low pass filter ──────────────────────────────────────────────────────────
float gx_f = 0, gy_f = 0, gz_f = 0;
float alpha = 0.2;

// ─── Last valid IMU readings ──────────────────────────────────────────────────
float last_ax = 0;

// ─── Session tracking ─────────────────────────────────────────────────────────
int total_reps  = 0;
int good_reps   = 0;
int total_score = 0;

static bool debug_nn = false;

// ─── Buffer ───────────────────────────────────────────────────────────────────
float buffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE] = {0};

// ─── Forward declaration ──────────────────────────────────────────────────────
String check_endpoint();

// ─── Quality score ────────────────────────────────────────────────────────────
int get_score(String quality) {
  if (quality == "good_raise")       return 100;
  if (quality == "arm_flat")         return 100;
  if (quality == "fast_raise")       return 50;
  if (quality == "jerky_raise")      return 40;
  if (quality == "incomplete_raise") return 20;
  return 0;
}

// ─── Read IMU safely ──────────────────────────────────────────────────────────
bool read_imu(float &ax, float &ay, float &az,
              float &gx, float &gy, float &gz) {
  if (IMU.accelerationAvailable() && IMU.gyroscopeAvailable()) {
    IMU.readAcceleration(ax, ay, az);
    IMU.readGyroscope(gx, gy, gz);
    last_ax = ax;
    return true;
  }
  return false;
}

// ─── Get arm position from physics ────────────────────────────────────────────
String get_position() {
  if (last_ax > 0.70) return "arm_down";
  if (last_ax < 0.30) return "arm_flat";
  return "arm_rising";
}

// ─── Collect 3 second window ──────────────────────────────────────────────────
void collect_window() {
  Serial.println("STATUS:collecting...");

  for (int i = 0; i < EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE; i += 6) {
    unsigned long next_tick = micros() + (1000000 / SAMPLE_FREQ_HZ);

    float ax, ay, az, gx, gy, gz;
    if (read_imu(ax, ay, az, gx, gy, gz)) {
      ax = constrain(ax, -MAX_ACCEPTED_RANGE, MAX_ACCEPTED_RANGE);
      ay = constrain(ay, -MAX_ACCEPTED_RANGE, MAX_ACCEPTED_RANGE);
      az = constrain(az, -MAX_ACCEPTED_RANGE, MAX_ACCEPTED_RANGE);

      gx_f = alpha * gx + (1 - alpha) * gx_f;
      gy_f = alpha * gy + (1 - alpha) * gy_f;
      gz_f = alpha * gz + (1 - alpha) * gz_f;

      buffer[i + 0] = ax * CONVERT_G_TO_MS2;
      buffer[i + 1] = ay * CONVERT_G_TO_MS2;
      buffer[i + 2] = az * CONVERT_G_TO_MS2;
      buffer[i + 3] = gx_f;
      buffer[i + 4] = gy_f;
      buffer[i + 5] = gz_f;
    }

    while (micros() < next_tick);
  }
}

// ─── Run inference ────────────────────────────────────────────────────────────
String run_inference() {
  signal_t signal;
  int err = numpy::signal_from_buffer(
              buffer,
              EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE,
              &signal);

  if (err != 0) {
    Serial.print("ERR: signal_from_buffer (");
    Serial.print(err); Serial.println(")");
    return "error";
  }

  ei_impulse_result_t result = {0};
  err = run_classifier(&signal, &result, debug_nn);

  if (err != EI_IMPULSE_OK) {
    Serial.print("ERR: run_classifier (");
    Serial.print(err); Serial.println(")");
    return "error";
  }

  float max_val     = 0;
  String best_label = "uncertain";

  Serial.print("CONF | ");
  for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
    float val = result.classification[i].value;
    Serial.print(result.classification[i].label);
    Serial.print(":");
    Serial.print(val * 100, 1);
    Serial.print("% ");

    if (val > max_val) {
      max_val    = val;
      best_label = result.classification[i].label;
    }
  }
  Serial.println();

  if (max_val < 0.75) return "uncertain";
  return best_label;
}

// ─── Check endpoint after window ──────────────────────────────────────────────
String check_endpoint() {
  float ax, ay, az;
  float ax_sum = 0;

  for (int i = 0; i < 10; i++) {
    if (IMU.accelerationAvailable()) {
      IMU.readAcceleration(ax, ay, az);
      ax_sum += ax;
      last_ax = ax;
    }
    delay(10);
  }

  float ax_avg = ax_sum / 10.0;

  Serial.print("ENDPOINT | ax_avg:");
  Serial.println(ax_avg, 2);

  if (ax_avg > 0.70) return "arm_down";    // never raised
  if (ax_avg > 0.25) return "incomplete";  // stopped halfway
  return "complete";                        // reached flat
}

// ─── Print session summary ────────────────────────────────────────────────────
void print_summary() {
  Serial.println("================================");
  Serial.println("       SESSION SUMMARY");
  Serial.println("================================");
  Serial.print("Total reps:    "); Serial.println(total_reps);
  Serial.print("Good reps:     "); Serial.println(good_reps);
  if (total_reps > 0) {
    Serial.print("Avg score:     ");
    Serial.println(total_score / total_reps);
    Serial.print("Success rate:  ");
    Serial.print((good_reps * 100) / total_reps);
    Serial.println("%");
  }
  Serial.println("================================");
}

// ─── Setup ────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  while (!Serial);

  Serial.println("Rehab Quality Assessment System");
  Serial.println("================================");

  if (!IMU.begin()) {
    Serial.println("ERR: IMU failed!");
    while (1);
  }

  if (EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME != 6) {
    Serial.print("ERR: Expected 6 axes, got ");
    Serial.println(EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME);
    while (1);
  }

  Serial.println("Ready — hold arm DOWN to begin");
  Serial.println("---");
}

// ─── Main loop ────────────────────────────────────────────────────────────────
void loop() {

  // ── Stage 1: Wait for arm to be DOWN ──────────────────────────────────────
  {
    float ax, ay, az, gx, gy, gz;
    read_imu(ax, ay, az, gx, gy, gz);

    if (get_position() != "arm_down") {
      Serial.print("WAITING | POS:");
      Serial.print(get_position());
      Serial.print(" AX:");
      Serial.println(last_ax, 2);
      delay(100);
      return;
    }
  }

  // ── Stage 2: Arm is DOWN — wait for raise to begin ────────────────────────
  Serial.println("STATUS:arm_down — ready for next rep");
  Serial.println("GESTURE:arm_down,CONF:100");
  Serial.println("STATE:rest");

  while (true) {
    float ax, ay, az, gx, gy, gz;
    if (read_imu(ax, ay, az, gx, gy, gz)) {
      if (get_position() != "arm_down") {
        Serial.println("STATUS:movement detected — starting classification");
        break;
      }
    }
    delay(20);
  }

  // ── Stage 3: Collect 3 second window ──────────────────────────────────────
  collect_window();

  // ── Stage 4: Inference + endpoint check ───────────────────────────────────
  String quality  = run_inference();
  String endpoint = check_endpoint();

  // Physics overrides — endpoint is ground truth
  if (endpoint == "arm_down") {
    quality = "uncertain";
    Serial.println("OVERRIDE: arm barely moved — not a rep");
  } else if (endpoint == "incomplete") {
    quality = "incomplete_raise";
    Serial.println("OVERRIDE: endpoint halfway → incomplete_raise");
  } else if (endpoint == "complete" && quality == "incomplete_raise") {
    quality = "good_raise";
    Serial.println("OVERRIDE: endpoint complete → upgrading to good_raise");
  }

  // ── Stage 5: Calculate score then send result ──────────────────────────────
  // score declared HERE after quality is finalised
  int score = get_score(quality);

  if (quality == "uncertain" || quality == "error") {
    Serial.println("STATUS:uncertain — try again");
    Serial.println("FEEDBACK:Movement not clear — try again");

  } else {
    // Update session
    total_reps++;
    total_score += score;
    if (score >= 80) good_reps++;

    // Send full result packet
    Serial.print("REP:");    Serial.print(total_reps);
    Serial.print(",QUAL:");  Serial.print(quality);
    Serial.print(",SCORE:"); Serial.print(score);
    Serial.print(",AX:");    Serial.println(last_ax, 2);

    // Simulation state
    if      (quality == "good_raise")       Serial.println("STATE:drawing_good");
    else if (quality == "fast_raise")       Serial.println("STATE:drawing_fast");
    else if (quality == "jerky_raise")      Serial.println("STATE:drawing_jerky");
    else if (quality == "incomplete_raise") Serial.println("STATE:drawing_incomplete");
    else if (quality == "arm_flat")         Serial.println("STATE:fullDraw");

    // Patient feedback
    if (score >= 80) {
      Serial.println("FEEDBACK:Great movement! Keep going!");
    } else if (quality == "fast_raise") {
      Serial.println("FEEDBACK:Too fast — slow down for better therapy");
    } else if (quality == "jerky_raise") {
      Serial.println("FEEDBACK:Try to move smoothly without stopping");
    } else if (quality == "incomplete_raise") {
      Serial.println("FEEDBACK:Try to reach full extension next time");
    }

    // Every 5 reps print summary
    if (total_reps % 5 == 0) {
      print_summary();
    }
  }

  // ── Stage 6: Cooldown ─────────────────────────────────────────────────────
  Serial.println("STATUS:cooldown 2 seconds...");
  delay(COOLDOWN_MS);
  Serial.println("STATUS:ready for next rep");
}