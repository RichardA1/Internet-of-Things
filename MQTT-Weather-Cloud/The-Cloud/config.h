// Update these with values suitable for your network.

//enter your WIFI SSID and Password
const char* ssid = "Your SSID";
const char* password = "Your Password";

// Enter your MQTT server adderss or IP, MQTT username, and MQTT password.
const char* mqtt_server = "MyMQTTadress"; // Enter your MQTT server adderss or IP.
const char* mqtt_username = "MyMQTT"; //enter your MQTT username
const char* mqtt_password = "MyMQTTpass"; //enter your password
const int mqtt_port = 1883;

/**************************** FOR PubSub **************************************************/
String clientId = "CloudLight"; //change this to whatever you want to call your device

/************* MQTT TOPICS (change these topics as you wish)  **************************/
const char* light_state_topic = "cloud";
const char* light_set_topic = "cloud/set";
const char* weather_data_topic = "cloud/data";
