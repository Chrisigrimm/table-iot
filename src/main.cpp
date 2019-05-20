
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <AutoConnect.h>
#include <ThingsBoard.h>

#define BUTTON_UP_INPUT 16
#define BUTTON_DOWN_INPUT 14

#define BUTTON_UP_OUTPUT 5
#define BUTTON_DOWN_OUTPUT 4

#define EEPROM_ADDRESS 2048
#define ThingsBoardConnectTimeout 5000

// #define THINGSBOARD_SERVER "104.248.252.18"
// #define TOKEN "KJtiinhydrY2MAFMBdIG"

// Initialize ThingsBoard client
WiFiClient espClient;
// Initialize ThingsBoard instance
ThingsBoard thingsBoard(espClient);

ESP8266WebServer Server;

AutoConnect Portal(Server);
AutoConnectConfig Config; // Enable autoReconnect supported on v0.9.4
AutoConnectInput thingsBoardServerInput("thingsboard_server", "", "Server IP", "Server IP");
AutoConnectInput thingsBoardTokenInput("thingsboard_token", "", "Token", "Token");
ACSubmit(thingsBoardSave, "SAVE", "/thingsboard_setting");
AutoConnectAux thingsBoardSettings("/thingsboard_setting", "ThingsBoard Setting");

boolean invertButtons = false;
boolean blockButtons = false;

boolean forceUp = false;
boolean forceDown = false;

int tryToConnectAgainIn = 0;

struct ThingsBoardDataStruct
{
  char thingsBoardServer[32];
  char thingsBoardToken[32];
} thingsBoardData;

void rootPage()
{
  String content =
      "<html>"
      "<head>"
      "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
      "</head>"
      "<body>"
      "<h2 align=\"center\" style=\"color:blue;margin:20px;\">Hello, world</h2>"
      "<h3 align=\"center\" style=\"color:gray;margin:10px;\">{{DateTime}}</h3>"
      "<p style=\"text-align:center;\">Reload the page to update the time.</p>"
      "<p></p><p style=\"padding-top:15px;text-align:center\">" AUTOCONNECT_LINK(COG_24) "</p>"
                                                                                         "</body>"
                                                                                         "</html>";
  static const char *wd[7] = {"Sun", "Mon", "Tue", "Wed", "Thr", "Fri", "Sat"};
  struct tm *tm;
  time_t t;
  char dateTime[26];

  t = time(NULL);
  tm = localtime(&t);
  sprintf(dateTime, "%04d/%02d/%02d(%s) %02d:%02d:%02d.",
          tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
          wd[tm->tm_wday],
          tm->tm_hour, tm->tm_min, tm->tm_sec);
  content.replace("{{DateTime}}", String(dateTime));
  Server.send(200, "text/html", content);
}

void setup()
{
  delay(1000);
  Serial.begin(74880);
  Serial.println();

  // Enable saved past credential by autoReconnect option,
  // even once it is disconnected.
  Config.autoReconnect = true;
  Portal.config(Config);

  // Setup Pins
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(BUTTON_UP_INPUT, INPUT);
  pinMode(BUTTON_DOWN_INPUT, INPUT);
  pinMode(BUTTON_UP_OUTPUT, OUTPUT);
  pinMode(BUTTON_DOWN_OUTPUT, OUTPUT);

  // Load thingsBoard data
  EEPROM.begin(EEPROM_ADDRESS + sizeof(ThingsBoardDataStruct));
  EEPROM.get(EEPROM_ADDRESS, thingsBoardData);

  thingsBoardServerInput.value = String(thingsBoardData.thingsBoardServer);
  thingsBoardTokenInput.value = String(thingsBoardData.thingsBoardToken);

  Server.on("/", rootPage);
  thingsBoardSettings.add({thingsBoardServerInput, thingsBoardTokenInput, thingsBoardSave});
  Portal.join(thingsBoardSettings);

  if (Portal.begin())
  {
    Serial.println("WiFi connected: " + WiFi.localIP().toString());
  }

  thingsBoard.sendAttributeBool("InvertButtons", invertButtons);
  thingsBoard.sendAttributeBool("BlockButtons", blockButtons);
  thingsBoard.sendAttributeBool("ForceUp", forceUp);
  thingsBoard.sendAttributeBool("ForceDown", forceDown);
}

RPC_Response setInvertButtons(const RPC_Data &data)
{
  Serial.println("Received the set switch method");

  invertButtons = data["InvertButtons"];

  return RPC_Response();
}

RPC_Response setBlockButtons(const RPC_Data &data)
{
  Serial.println("Received the set switch method");

  blockButtons = data["BlockButtons"];

  return RPC_Response();
}

RPC_Response setForceUp(const RPC_Data &data)
{
  Serial.println("Received the set switch method");

  forceUp = data["ForceUp"];

  return RPC_Response();
}

RPC_Response setForceDown(const RPC_Data &data)
{
  Serial.println("Received the set switch method");

  forceDown = data["ForceDown"];

  return RPC_Response();
}

const size_t callbacks_size = 4;
RPC_Callback callbacks[callbacks_size] = {
    {"set_InvertButtons", setInvertButtons},
    {"set_BlockButtons", setBlockButtons},
    {"set_ForceUp", setForceUp},
    {"set_ForceDown", setForceDown}
};

bool subscribed = false;

void loop()
{
  delay(100);

  boolean buttonUp_Pressed = digitalRead(BUTTON_UP_INPUT);
  boolean buttonDown_Pressed = digitalRead(BUTTON_DOWN_INPUT);

  thingsBoard.sendTelemetryBool("ButtonUpInput", buttonUp_Pressed);
  thingsBoard.sendTelemetryBool("ButtonDownInput", buttonDown_Pressed);

  if (invertButtons)
  {
    boolean temp_buttonUp_Pressed = buttonUp_Pressed;
    buttonUp_Pressed = buttonDown_Pressed;
    buttonDown_Pressed = temp_buttonUp_Pressed;
  }

  //Input to Output Buttons
  if ((buttonUp_Pressed && !blockButtons) || forceUp)
  {
    digitalWrite(BUTTON_UP_OUTPUT, HIGH);
    thingsBoard.sendTelemetryBool("ButtonUpOutput", true);
  }
  else
  {
    digitalWrite(BUTTON_UP_OUTPUT, LOW);
    thingsBoard.sendTelemetryBool("ButtonUpOutput", false);
  }

  if ((buttonDown_Pressed && !blockButtons) || forceDown)
  {
    digitalWrite(BUTTON_DOWN_OUTPUT, HIGH);
    thingsBoard.sendTelemetryBool("ButtonDownOutput", true);
  }
  else
  {
    digitalWrite(BUTTON_DOWN_OUTPUT, LOW);
    thingsBoard.sendTelemetryBool("ButtonDownOutput", false);
  }

  // LED INDICATOR
  if ((buttonUp_Pressed || buttonDown_Pressed))
  {
    digitalWrite(LED_BUILTIN, LOW);
  }
  else
  {
    digitalWrite(LED_BUILTIN, HIGH);
  }

  if (!thingsBoard.connected() && tryToConnectAgainIn < millis())
  {
    tryToConnectAgainIn = millis() + ThingsBoardConnectTimeout;

    int thingsBoardServerLength = thingsBoardServerInput.value.length() + 1;
    int thingsBoardTokenLength = thingsBoardTokenInput.value.length() + 1;

    if (thingsBoardServerLength > 1 && thingsBoardTokenLength > 1)
    {
      char thingsBoardServer[thingsBoardServerLength];
      thingsBoardServerInput.value.toCharArray(thingsBoardServer, thingsBoardServerLength);
      char thingsBoardToken[thingsBoardTokenLength];
      thingsBoardTokenInput.value.toCharArray(thingsBoardToken, thingsBoardTokenLength);

      subscribed = false;
      // Connect to the ThingsBoard
      Serial.print("Connecting to: ");
      Serial.print(thingsBoardServer);
      Serial.print(" with token ");
      Serial.println(thingsBoardToken);
      if (!thingsBoard.connect(thingsBoardServer, thingsBoardToken))
      {
        Serial.println("Failed to connect");
        return;
      }

      // save thingsboard data
      Serial.println("Save ThingsBoard Data");

      strncpy(thingsBoardData.thingsBoardServer, thingsBoardServer, 32);
      strncpy(thingsBoardData.thingsBoardToken, thingsBoardToken, 32);

      EEPROM.put(EEPROM_ADDRESS, thingsBoardData);
      // commit (write) the data to EEPROM - only actually writes if there has been a change
      EEPROM.commit();
    }
  }
  
  
  if(thingsBoard.connected())
  {
    if (!subscribed)
    {
      Serial.println("Subscribing for RPC...");

      // Perform a subscription. All consequent data processing will happen in
      // processTemperatureChange() and processSwitchChange() functions,
      // as denoted by callbacks[] array.
      if (!thingsBoard.RPC_Subscribe(callbacks, callbacks_size))
      {
        Serial.println("Failed to subscribe for RPC");
        return;
      }

      Serial.println("Subscribe done");
      subscribed = true;
    }
  }

  thingsBoard.loop();
  Portal.handleClient();
}
