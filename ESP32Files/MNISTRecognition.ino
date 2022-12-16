#include "image_processing.h"
#include "model_settings.h"
#include "model.h"
#include "w1.h"
#include "w2.h"
#include "w3.h"
#include "w4.h"

#define MQTT_PUBSUB_LOGLEVEL              1
#include <WebSocketsClient_Generic.h> // For websocket 
#include <MQTTPubSubClient_Generic.h> // For MQTT Client
#include <WiFi.h>
#include <ArduinoJson.h>
#include "base64.h"


WebSocketsClient client;
MQTTPubSub::PubSubClient <1000> mqttClient;
//
//#define WS_SERVER         "192.168.0.198"
//String path = "/";
String WS_SERVER = "noderedlnxse20220516.o1jkfiv0l98.eu-de.codeengine.appdomain.cloud";
String path = "/ws/mqtt";

#define WS_PORT             443
const char *PubTopic    = "RESULT";
const char *ListenTopic    = "COMMAND";
String encoded_image;
const char* ssid = "Advtopic";//"The Promised LAN";

const char* password = "Adv123Topic";// "1234567883";
uint8_t image_data[784];
String outString;
float maxValue = 0;
int maxIndex = 0;
bool make_prediction = false;
String Hostname = "ESP32 Usama Node";
String out;


float leakyRelu(float a)
{
  if (a > 0)
    return a;
  else
    return 0.3 * a;
}

float Sigmoid(float z)
{
  return 1 / (1 + exp(-z));

}

void WifiAndMqttInit() {
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
  WiFi.setHostname(Hostname.c_str());
  Serial.printf("WIFI begin ");
  WiFi.begin(ssid, password);
  Serial.begin(115200);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.printf("WIFI end ");
  client.beginSSL(WS_SERVER, WS_PORT, path);
  client.setReconnectInterval(1000);
  // initialize mqtt client
  mqttClient.begin(client);

  mqttClient.connect(Hostname);

}


//Neural Network for prediction of output
void prediction()
{
  int layers = 3;
  //The weights obtained by training

  const int numberOuts = 10;
  //The biases obstained by training
  //Inputs and outputs
  double output[numberOuts];
  int act [] = {2, 0};//Relu and sigmoid
  float hidden_nodes[topology[1]] = {0};
  float hidden_nodes2[topology[2]] = {0};
  float hidden_nodes3[topology[3]] = {0};
  float outNodes[topology[4]] = {0};
  int temp;

  Serial.println("First Layer");
  //First neural layer
  for (int i = 0; i < topology[1]; i++)
  {
    //Divided by thousand as weights are saved after multiplying by 1000 as int to save memory rather than floats
    hidden_nodes[i] = static_cast<float>(b1[i] / 1000);
    for (int j = 0; j < topology[0]; j++)
    {
      hidden_nodes[i] += static_cast<float>(w1[j][i] * image_data[j] / 1000) ;
    }
    hidden_nodes[i] = leakyRelu(hidden_nodes[i]);
    //Serial.print(hidden_nodes[i]);
  }

  Serial.println("Second Layer");
  // Second layer
  for (int i = 0; i < topology[2]; i++)
  {
    hidden_nodes2[i] = b2[i] / 1000;
    for (int j = 0; j < topology[1]; j++)
    {
      hidden_nodes2[i] += static_cast<float>(w2[j][i] * hidden_nodes[j] / 1000);
    }
    hidden_nodes2[i] = leakyRelu(hidden_nodes2[i]);
    //    Serial.print(hidden_nodes2[i]);
  }


  Serial.println("Three Layer");
  for (int i = 0; i < topology[3]; i++)
  {
    hidden_nodes3[i] = static_cast<float> (b3[i] / 1000);
    for (int j = 0; j < topology[2]; j++)
    {
      hidden_nodes3[i] += static_cast<float>(w3[j][i] * hidden_nodes2[j] / 1000);
    }
    hidden_nodes3[i] = leakyRelu(hidden_nodes3[i]);
    // Serial.print(hidden_nodes3[i]);
  }

  Serial.println("Output Layer");
  for (int i = 0; i < topology[4]; i++)
  {
    outNodes[i] = static_cast<float> (b4[i] / 1000);

    for (int j = 0; j < topology[3]; j++)
    {
      outNodes[i] += static_cast<float>(w4[j][i] * hidden_nodes3[j] / 1000);
    }
    //Serial.print(outNodes[i]);


  }


  maxValue = -900;
  maxIndex = 0;

  //Get the max value from the output nodes
  for (int i = 0; i < topology[4]; i++) {

    if (maxValue <= outNodes[i])
    {
      maxValue = outNodes[i];
      maxIndex = i;
    }
    //outNodes[i] = Sigmoid(outNodes[i]);

    //Serial.println(outNodes[i], 4);
  }

  Serial.println(maxValue);
  Serial.println(maxIndex);
}


bool recognize = false;
bool listenMessage()
{
  recognize = false;
  mqttClient.subscribe([](const String & topic, const String & payload, const size_t size)
  {
    (void) size;

    Serial.println("MQTT received: " + topic + " - " + payload);

    if (topic == "COMMAND" && (payload == "Recognize" || payload == "Rec."))
    {
      Serial.println("Success");
      recognize = true;
    }

    //return payload;
  });

  // subscribe topic and callback which is called when the arrived packet has come
  mqttClient.subscribe(ListenTopic, [](const String & payload, const size_t size)
  {
    (void) size;
    Serial.print("Subcribed to ");
    Serial.print(ListenTopic); Serial.print(" => ");
    Serial.print(payload);
    if (ListenTopic == "COMMAND" && (payload == "Recognize" || payload == "Rec."))
    {
      Serial.println("Success");
      recognize = true;
    }
  });
  return recognize;
}


void publish_mqtt(String message) {

  Serial.printf("Mqtt begin");
  Serial.print("Connecting to secured-host:port = "); Serial.print(WS_SERVER);
  Serial.print(":"); Serial.println(WS_PORT);
  Serial.print("Connecting to mqtt broker...");

  // Wait for the client to connect to ensure packet reach its destination
  while (!mqttClient.connect(Hostname))
  {
    Serial.print(".");
    delay(1000);
  }

  Serial.println(" connected!");
  // subscribe callback which is called when every packet has come
  mqttClient.subscribe([](const String & topic, const String & payload, const size_t size)
  {
    (void) size;

    Serial.println("MQTT received: " + topic + " - " + payload);
    if (payload == "Recognize")
    {
      Serial.println("Success");
      make_prediction = true;
    }
  });


  //See if there is a new command
  mqttClient.subscribe(ListenTopic, [](const String & payload, const size_t size)
  {
    (void) size;
    Serial.print("Subcribed to ");
    Serial.print(ListenTopic); Serial.print(" => ");
    if (payload == "Recognize")
    {
      Serial.println("Success");
      make_prediction = true;
    }
  });

  // subscribe to see if the data has been published
  mqttClient.subscribe(PubTopic, [](const String & payload, const size_t size)
  {
    (void) size;
    Serial.print("Subcribed to ");
    Serial.print(PubTopic); Serial.print(" => ");
    if (payload == "Recognize")
    {
      Serial.println("Success");
      make_prediction = true;
    }
  });


  mqttClient.publish(PubTopic, message);
  Serial.println("Message published");
  Serial.printf("mqtt end");
}



//Sent the prediction to MQTT broker
void sent_prediction(int index, float value)
{
  //Take to sigmoid in order the probability
  value = Sigmoid(value);
  String out;
  DynamicJsonDocument doc(100);
  doc["id"] = 3751313;
  doc["probability"] = value;
  doc["prediction"] = index;
  serializeJson(doc, out);
  while (!mqttClient.connect("arduino"))
  {
    Serial.print(".");
    delay(1000);
  }
  publish_mqtt(out);
}

void setup() {

  //Initialize the wifi and MQTT connection
  WifiAndMqttInit();
}


void loop() {

  if (make_prediction)
  {
    if (true != GetImage(kNumCols, kNumRows, kNumChannels,
                         image_data)) {
      Serial.println("Faield in main loop");
    }
    else {
      Serial.println("Making prediction");
      //Run the neural network
      prediction();

      //Prediction and probability are stored in these global variables
      sent_prediction(maxIndex, maxValue);
    }

    delay(2000);
  }

  //Do not make prediction unless asked by COMMAND message from MQTT
  make_prediction = false;

  //See if there is any message from the broker
  mqttClient.update();

}
