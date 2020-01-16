#ifdef AQS_PRESENT
#include <SparkFunCCS811.h>
#endif

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Servo.h>

#include "definitions0.h"

// Connect to the WiFi
const char* ssid = "CMU-DEVICE";
const char* mqtt_server = "api.daybook.space";
 
WiFiClient espClient;
PubSubClient client(espClient);

Servo left_servo;
Servo right_servo;
Servo pitch_servo;
Servo yaw_servo;
Servo lock_servo;

#ifdef AQS_PRESENT
CCS811 aqs(AQS_ADDR);
bool aqs_available = false;
int co2 = -1;
int tvoc = -1;
#endif

void lock(String payload) {
  int lock_state = payload.toInt();

  if (lock_state) {
    Serial.println("Locking...");
    lock_servo.write(LOCKED_POS);
  } else {
    Serial.println("Unlocking...");
    lock_servo.write(UNLOCKED_POS);
  }

  delay(15);
}

int speed_to_angle(int speed_, int zero_point) {
  speed_ = (speed_ * 9) / 10;
  return constrain(speed_ + zero_point, 0, 180);
}

void left(String payload) {
  int speed_ = payload.toInt();

  if (speed_ < -100 || speed_ > 100) {
    Serial.println("Invalid speed");
    return;
  }

  Serial.print("Setting left wheel to speed ");
  Serial.println(payload);

  speed_ = speed_to_angle(speed_, LEFT_ZERO);
  left_servo.write(REVERSE_LEFT ? 180 - speed_ : speed_);
}

void right(String payload) {
  int speed_ = payload.toInt();

  if (speed_ < -100 || speed_ > 100) {
    Serial.println("Invalid speed");
    return;
  }

  Serial.print("Setting right wheel to speed ");
  Serial.println(payload);

  speed_ = speed_to_angle(speed_, RIGHT_ZERO);
  right_servo.write(REVERSE_RIGHT ? 180 - speed_ : speed_);
}

void tank(String payload) {
  if (payload.indexOf(' ') <= 0) {
    Serial.println("Invalid tank command...");
    return;
  }

  String left_speed = payload.substring(0, payload.indexOf(' '));
  String right_speed = payload.substring(payload.indexOf(' ') + 1);
  
  Serial.print("Setting tank drive to (");
  Serial.print(left_speed);
  Serial.print(", ");
  Serial.print(right_speed);
  Serial.println(")");

  left(left_speed);
  right(right_speed);
}

void yaw(String payload) {
  int angle = payload.toInt();

  if (angle < -60 || angle > 60) {
    Serial.println("Invalid yaw angle");
    return;
  }

  Serial.print("Setting yaw to ");
  Serial.print(angle);
  Serial.println(" degrees");

  angle += YAW_HOME;

  yaw_servo.write(constrain(REVERSE_YAW ? 180 - angle : angle, 0, 180));
}

void pitch(String payload) {
  int angle = payload.toInt();

  if (angle < -45 || angle > 90) {
    Serial.println("Invalid pitch angle");
    return;
  }

  Serial.print("Setting pitch to ");
  Serial.print(angle);
  Serial.println(" degrees");

  angle += PITCH_HOME;

  pitch_servo.write(constrain(REVERSE_PITCH ? 180 - angle : angle, 0, 180));
}

void home_servos() {
  Serial.println("Homing servos...");

  pitch_servo.write(PITCH_HOME);
  yaw_servo.write(YAW_HOME);
  lock_servo.write(UNLOCKED_POS);
  left_servo.write(LEFT_ZERO);
  right_servo.write(RIGHT_ZERO);

  delay(15);
}

void distance(String payload) {
  long duration, distance;
  char* pub_mess = (char*)malloc(sizeof(char) * 5);
  
  digitalWrite(DIST_TRIG, LOW);  // Added this line
  delayMicroseconds(2); // Added this line
  digitalWrite(DIST_TRIG, HIGH);
  delayMicroseconds(10); // Added this line
  digitalWrite(DIST_TRIG, LOW);
  duration = pulseIn(DIST_ECHO, HIGH);
  distance = (duration/2) / 29.1;

  Serial.print("Measured distance ");
  Serial.print(distance);
  Serial.println(" cm");

  String(distance).toCharArray(pub_mess, 5);

  client.publish("robot_return/" ESP_ID "/distance", pub_mess);

  free(pub_mess);
}

#ifdef AQS_PRESENT
void air(String payload) {
  if (!aqs_available) {
    Serial.println("Air quality sensor not connected!");
  }
  
  Serial.println("Getting air quality data...");
  if (aqs.dataAvailable())
  {
    //If so, have the sensor read and calculate the results.
    //Get them later
    aqs.readAlgorithmResults();

    Serial.print("CO2[");
    //Returns calculated CO2 reading
    co2 = aqs.getCO2();
    Serial.print(co2);
    Serial.print("] tVOC[");
    //Returns calculated TVOC reading
    tvoc = aqs.getTVOC();
    Serial.print(tvoc);
    Serial.println();
  } else if (co2 == -1 || tvoc == -1) {
    Serial.println("Data unavailable");
    client.publish("robot_return/" ESP_ID "/air", "data_unavailable");
    return;
  }

  char* co2_mess = (char*)malloc(sizeof(char) * 6);
  char* tvoc_mess = (char*)malloc(sizeof(char) * 6);

  String(co2).toCharArray(co2_mess, 6);
  String(tvoc).toCharArray(tvoc_mess, 6);

  client.publish("robot_return/" ESP_ID "/air/co2", co2_mess);
  client.publish("robot_return/" ESP_ID "/air/tvoc", tvoc_mess);

  free(co2_mess);
  free(tvoc_mess);
}
#endif

void callback(char* topic, byte* payload, unsigned int length) {
   Serial.print("Message arrived [");
   Serial.print(topic);
   Serial.print("] ");

   payload[length] = 0;

   String topicStr(topic);
   String payloadStr((char*)payload);

   Serial.print(payloadStr);
   Serial.println();

   if (topicStr.indexOf('/') >= 0) {
    topicStr.remove(0, topicStr.indexOf('/') + 1);
   } else {
    Serial.println("Invalid message...");
    return;
   }

   if (topicStr.indexOf('/') >= 0) {
    topicStr.remove(0, topicStr.indexOf('/') + 1);
   } else {
    Serial.println("Invalid message...");
    return;
   }

   Serial.print("Command: ");
   Serial.println(topicStr);

   if (topicStr.equals("lock")) {
    lock(payloadStr);
   } else if (topicStr.equals("tank")) {
    tank(payloadStr);
   } else if (topicStr.equals("left")) {
    left(payloadStr);
   } else if (topicStr.equals("right")) {
    right(payloadStr);
   } else if (topicStr.equals("pitch")) {
    pitch(payloadStr);
   } else if (topicStr.equals("yaw")) {
    yaw(payloadStr);
   } else if (topicStr.equals("home")) {
    home_servos();
   } else if (topicStr.equals("distance")) {
    distance(payloadStr);
   #ifdef AQS_PRESENT
   } else if (topicStr.equals("air")) {
    air(payloadStr);
   #endif
   } else {
    Serial.println("Invalid command");
   }
   
   Serial.println();
}

void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void setup_servos() {
  Serial.println();
  Serial.println("Setting up servos...");
  
  left_servo.attach(LEFT_SERVO);
  right_servo.attach(RIGHT_SERVO);
  pitch_servo.attach(PITCH_SERVO);
  yaw_servo.attach(YAW_SERVO);
  lock_servo.attach(LOCK_SERVO);
}

void setup_ultrasonic() {
  Serial.println("Setting up ultrasonic sensor...");
  pinMode(DIST_TRIG, OUTPUT);
  pinMode(DIST_ECHO, INPUT);
}

#ifdef AQS_PRESENT
void setup_aqs() {
  Serial.print("Setting up air quality sensor... ");
  if (!aqs.begin()) {
    Serial.println("FAILED to start sensor! Check wiring. AQS disabled.");
    return;
  }

  aqs_available = true;
  Serial.println("Connected!");
  aqs.setDriveMode(1);
}
#endif

void reconnect() {
   // Loop until we're reconnected
   while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("NaaginBot " ESP_ID)) {
      Serial.println("connected");
      // ... and subscribe to topic
      client.subscribe("robot_cmd/" ESP_ID "/#");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
  Serial.println();
}
 
void setup()
{
 Serial.begin(115200);

 Serial.println("-----NAAGIN COMPUTE MODULE RESET-----");
 Serial.println("Naagin Firmware v1.02");
 Serial.println("This is Naagin " ESP_ID);

#ifdef AQS_PRESENT
 Serial.println("INFO: Air Quality Sensor is set to installed");
#endif

 setup_wifi();
 setup_servos();
 setup_ultrasonic();
 home_servos();
 
#ifdef AQS_PRESENT
 setup_aqs();
#endif
 
 client.setServer(mqtt_server, 1883);
 client.setCallback(callback);
}
 
void loop()
{
 if (!client.connected()) {
  reconnect();
 }
 client.loop();
}
