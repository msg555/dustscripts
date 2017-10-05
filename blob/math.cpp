const float PI = 3.14159265358979;

float lerp(float a, float b, float x) {
  return a * (1.0 - x) + b * x;
}

float degtorad(float x) {
  return x / 180.0 * PI;
}

float radtodeg(float x) {
  return x * 180.0 / PI;
}

float distance(float x1, float y1, float x2, float y2) {
  return sqrt((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2));
}

float lengthdir_x(float r, float ang) {
  return r * sin(degtorad(ang));
}

float lengthdir_y(float r, float ang) {
  return -r * cos(degtorad(ang));
}

float point_angle(float x1, float y1, float x2, float y2) {
  return radtodeg(atan2(x2 - x1, y1 - y2));
}

float sqr(float x) {
  return x * x;
}

float sgn(float x) {
  if (x < -1e-9) {
    return -1;
  } else if (x > 1e-9) {
    return 1;
  }
  return 0;
}
