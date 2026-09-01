// Arduino Uno R4 WiFi Rover Controller
// Hardware:
//   - Uno R4 WiFi
//   - Drok L298N dual motor driver
//   - 2 DC motors
//   - MPU6050 accelerometer/gyro
//   - 12V fuse + SPST power switch on main battery line
//
// Wiring used by this sketch:
//   Left motor:  IN1 = 8, IN2 = 9, ENA = 5
//   Right motor: IN3 = 12, IN4 = 13, ENB = 6
//   MPU6050: SDA = A4, SCL = A5
//   12V fuse and SPST switch must be placed on the battery positive lead before the motor driver.
//
// Phone interface:
//   Connect phone to the rover WiFi hotspot on "RoverAP" and open the IP shown in Serial Monitor.
//   Example controls: Forward, Reverse, Left, Right, Stop, Speed slider.

#include <Wire.h>
#include <MPU6050_light.h>
#include <WiFiS3.h>

// ---- Motor pins ----
const int LEFT_IN1 = 8;
const int LEFT_IN2 = 9;
const int LEFT_ENA = 5;

const int RIGHT_IN3 = 12;
const int RIGHT_IN4 = 13;
const int RIGHT_ENB = 6;

// ---- Rover settings ----
const int DEFAULT_SPEED = 180;
const int MIN_SPEED = 0;
const int MAX_SPEED = 255;

int currentSpeed = DEFAULT_SPEED;

// ---- WiFi access point ----
const char* ssid = "RoverAP";
const char* password = "rover123";
WiFiServer server(80);

// ---- IMU ----
MPU6050 mpu(Wire);
float pitch = 0.0;
float roll = 0.0;

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(LEFT_IN1, OUTPUT);
  pinMode(LEFT_IN2, OUTPUT);
  pinMode(LEFT_ENA, OUTPUT);

  pinMode(RIGHT_IN3, OUTPUT);
  pinMode(RIGHT_IN4, OUTPUT);
  pinMode(RIGHT_ENB, OUTPUT);

  stopMotors();

  Wire.begin();
  mpu.begin();
  delay(500);
  mpu.calcOffsets();
  Serial.println("MPU6050 initialized.");

  Serial.println("Starting Rover WiFi AP...");
  WiFi.beginAP(ssid, password);
  server.begin();

  Serial.print("Rover AP name: ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  Serial.println("Use your phone to connect and open the IP address in a browser.");
}

void loop() {
  mpu.update();
  pitch = mpu.getAngleX();
  roll = mpu.getAngleY();

  WiFiClient client = server.available();
  if (client) {
    handleClient(client);
  }

  // Optional: allow basic commands over Serial monitor if needed.
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    handleCommand(cmd);
  }
}

void handleClient(WiFiClient &client) {
  String request = "";
  while (client.connected() && client.available()) {
    char c = client.read();
    request += c;
    if (c == '\n' && request.indexOf("\r\n\r\n") != -1) {
      break;
    }
  }

  if (request.length() == 0) {
    return;
  }

  String response = buildWebPage();

  if (request.indexOf("GET /?cmd=F") >= 0) {
    handleCommand("F");
  } else if (request.indexOf("GET /?cmd=B") >= 0) {
    handleCommand("B");
  } else if (request.indexOf("GET /?cmd=L") >= 0) {
    handleCommand("L");
  } else if (request.indexOf("GET /?cmd=R") >= 0) {
    handleCommand("R");
  } else if (request.indexOf("GET /?cmd=S") >= 0) {
    handleCommand("S");
  } else if (request.indexOf("GET /?cmd=") >= 0) {
    // If the command is not recognized, just keep the rover stopped.
    stopMotors();
  }

  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.print("Content-Length: ");
  client.println(response.length());
  client.println();
  client.print(response);
  delay(10);
  client.stop();
}

void handleCommand(String command) {
  command.trim();
  if (command.length() == 0) {
    return;
  }

  Serial.print("Command: ");
  Serial.println(command);

  if (command.startsWith("SPEED")) {
    String value = command.substring(5);
    int speedValue = value.toInt();
    currentSpeed = constrain(speedValue, MIN_SPEED, MAX_SPEED);
    Serial.print("Speed set to: ");
    Serial.println(currentSpeed);
    return;
  }

  if (command.startsWith("M,")) {
    int comma1 = command.indexOf(',');
    int comma2 = command.indexOf(',', comma1 + 1);
    if (comma1 >= 0 && comma2 > comma1) {
      int leftValue = command.substring(comma1 + 1, comma2).toInt();
      int rightValue = command.substring(comma2 + 1).toInt();
      drive(leftValue, rightValue);
      return;
    }
  }

  switch (command.charAt(0)) {
    case 'F':
      drive(currentSpeed, currentSpeed);
      break;
    case 'B':
      drive(-currentSpeed, -currentSpeed);
      break;
    case 'L':
      drive(-currentSpeed, currentSpeed);
      break;
    case 'R':
      drive(currentSpeed, -currentSpeed);
      break;
    case 'S':
      stopMotors();
      break;
    default:
      Serial.println("Unknown command");
      break;
  }
}

void drive(int leftValue, int rightValue) {
  setMotor(LEFT_IN1, LEFT_IN2, LEFT_ENA, leftValue);
  setMotor(RIGHT_IN3, RIGHT_IN4, RIGHT_ENB, rightValue);
}

void setMotor(int in1, int in2, int enablePin, int value) {
  int pwm = abs(value);
  pwm = constrain(pwm, 0, 255);

  if (value > 0) {
    digitalWrite(in1, HIGH);
    digitalWrite(in2, LOW);
  } else if (value < 0) {
    digitalWrite(in1, LOW);
    digitalWrite(in2, HIGH);
  } else {
    digitalWrite(in1, LOW);
    digitalWrite(in2, LOW);
    analogWrite(enablePin, 0);
    return;
  }

  analogWrite(enablePin, pwm);
}

void stopMotors() {
  analogWrite(LEFT_ENA, 0);
  analogWrite(RIGHT_ENB, 0);
  digitalWrite(LEFT_IN1, LOW);
  digitalWrite(LEFT_IN2, LOW);
  digitalWrite(RIGHT_IN3, LOW);
  digitalWrite(RIGHT_IN4, LOW);
}

String buildWebPage() {
  String page = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
  page += "<title>Rover Control</title><style>body{font-family:Arial;background:#111;color:#fff;text-align:center;padding:20px}button{width:110px;height:60px;margin:8px;font-size:22px;border:none;border-radius:12px;background:#2a7fff;color:white}#wrap{max-width:360px;margin:auto}.row{display:flex;justify-content:center}.slider{width:90%;margin:20px 0}</style></head><body>";
  page += "<div id='wrap'><h2>Rover Control</h2><div class='row'><button onclick=location.href='/?cmd=F'>F</button></div>";
  page += "<div class='row'><button onclick=location.href='/?cmd=L'>L</button><button onclick=location.href='/?cmd=S'>S</button><button onclick=location.href='/?cmd=R'>R</button></div>";
  page += "<div class='row'><button onclick=location.href='/?cmd=B'>B</button></div>";
  page += "<input class='slider' type='range' min='0' max='255' value='180' oninput='location.href=`/?cmd=SPEED`+this.value' />";
  page += "<p>Pitch: " + String(pitch, 1) + "&deg; | Roll: " + String(roll, 1) + "&deg;</p>";
  page += "</div></body></html>";
  return page;
}
