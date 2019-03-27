#define MOTOR_A 12
#define MOTOR_B 11
#define IR_A 9
#define IR_B 10

/*
 * 4 IR ticks/input_revolution
 * 75.81 input_revolution/output_revolution
 * 360 degrees/output_revolution
 * Therefore, (4 ticks/input_rev * 75.81 input_rev/output_rev)/360 degrees/output_rev = 0.8423 ticks/degree
 * and 360 degrees/output_rev/(4 ticks/input_rev * 75.81 input_rev/output_rev) = 1.1872 degrees/tick
 */
const double ticks_per_degree = (4.0 * 75.81) / 360.0,  // 0.8423
             degrees_per_tick = 360.0 / (4.0 * 75.81),  // 1.1872
             K_p = 0.1;

long desired_angle, angle, desired_ticks, ticks, error, cur_speed;
unsigned long tick_time, tock_time, desired_control_time = 50000;
short new_input = 1;

int set_speed(float s);
void set_velocity(float v);

// Enumeration of possible rotation directions
volatile enum dir
{
  CW = -1,
  CCW = 1
} dir_cur;

// Struct to store values of both IR sensors for a given state
struct state
{
  short a;
  short b;
} state_prev, state_cur;

void setup()
{
  Serial1.begin(9600);
  pinMode(MOTOR_A, OUTPUT);
  pinMode(MOTOR_B, OUTPUT);
  pinMode(IR_A, INPUT);
  pinMode(IR_B, INPUT);
  attachInterrupt(digitalPinToInterrupt(IR_A), ir_isr, CHANGE);
  attachInterrupt(digitalPinToInterrupt(IR_B), ir_isr, CHANGE);
  state_cur.a = digitalRead(IR_A);
  state_cur.b = digitalRead(IR_B);
}

void loop()
{
  while(1)
  {
    // Get time at beginning of P controller loop
    tick_time = micros();

    // Wait for angle to be sent via serial
    if (new_input)
    {
      if (Serial1.available())
      {
        desired_angle = Serial1.parseInt();
        desired_ticks = (int)((double)desired_angle * ticks_per_degree);
        if (desired_ticks < 0)
          desired_ticks *= -1;
        ticks = 0;
        if (desired_angle > 0)
          dir_cur = CCW;
        else
          dir_cur = CW;
        new_input = 0;
      }
    }

    // Actually try getting to that angle
    else
    {
      error = desired_ticks - ticks;
      cur_speed = error * K_p * dir_cur;
      set_velocity(cur_speed);
      if (error < 3)
        new_input = 1;
    }

    // Get time at end of P controller loop
    tock_time = micros();

    // Ensure at least the minimum delay has occured for P controller loop
    delayMicroseconds(desired_control_time - (tock_time - tick_time));
  }
}

// Maps s to corresponding int value on 0-255 scale (clamps values bigger/smaller than 1.0/0.5 to 1.0/0.5, respectively).
int set_speed(float s)
{
  if (s < 0.5f)
    s = 0.5f;
  else if (s > 1.0f)
    s = 1.0f;
  return (int)(s * 255.0f);
}

// Outputs PWM on either MOTOR_A or MOTOR_B pins depending on sign
void set_velocity(float v)
{
  if (v < 0.0f)
  {
    analogWrite(MOTOR_A, set_speed(-1.0f * v));
    analogWrite(MOTOR_B, 0);
  }

  else if (v > 0.0f)
  {
    analogWrite(MOTOR_A, 0);
    analogWrite(MOTOR_B, set_speed(v));
  }

  else
  {
    analogWrite(MOTOR_A, 0);
    analogWrite(MOTOR_B, 0);
  }
}

// ISR for every edge of the IR sensors that also uses a state machine to determine current direction.
void ir_isr()
{
  // Shift current state to previous state
  state_prev.a = state_cur.a;
  state_prev.b = state_cur.b;

  // Save new current state
  state_cur.a = digitalRead(IR_A);
  state_cur.b = digitalRead(IR_B);
  if (error < 0.0)
    ticks--;
  else
    ticks++;

  if (!state_prev.a && !state_prev.b)
  {
    if (!state_cur.a && state_cur.b)
      dir_cur = CW;
    else
      dir_cur = CCW;
  }

  else if (!state_prev.a && state_prev.b)
  {
    if (state_cur.a && state_cur.b)
      dir_cur = CW;
    else
      dir_cur = CCW;
  }

  else if (state_prev.a && !state_prev.b)
  {
    if (!state_cur.a && !state_cur.b)
      dir_cur = CW;
    else
      dir_cur = CCW;
  }

  else
  {
    if (state_cur.a && !state_cur.b)
      dir_cur = CW;
    else
      dir_cur = CCW;
  }
}
