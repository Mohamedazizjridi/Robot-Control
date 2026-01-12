/*********
  Robot Control with ASCON128 Encryption
  ESP32 Secure WiFi-based motor control
  with HTTP Basic Authentication
*********/

#include <WiFi.h>
#include <Crypto.h>
#include <Ascon128.h>

/* ================= WIFI ================= */
const char* ssid     = "Redmi Note 13 Pro";
const char* password = "anas1234567890";
WiFiServer server(80);

/* ================= AUTHENTICATION ================= */
const char* authUsername = "admin";
const char* authPassword = "robot2024";

// Base64 decoding
String base64_decode(String input) {
  const char* base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  String output = "";
  
  // Remove any whitespace
  input.trim();
  
  int val = 0;
  int valb = -8;
  
  for (int i = 0; i < input.length(); i++) {
    char c = input.charAt(i);
    
    if (c == '=') break;
    if (c == ' ' || c == '\r' || c == '\n') continue;
    
    // Find position in base64 alphabet
    int pos = -1;
    for (int j = 0; j < 64; j++) {
      if (base64_chars[j] == c) {
        pos = j;
        break;
      }
    }
    
    if (pos == -1) continue;
    
    val = (val << 6) | pos;
    valb += 6;
    
    if (valb >= 0) {
      output += char((val >> valb) & 0xFF);
      valb -= 8;
    }
  }
  
  return output;
}

// Function to check Basic Auth
bool checkAuthentication(String header) {
  // Find Authorization header line
  int authIndex = header.indexOf("Authorization:");
  if (authIndex == -1) {
    Serial.println("No Authorization header found");
    return false;
  }
  
  // Extract the line
  int lineEnd = header.indexOf("\r\n", authIndex);
  String authLine = header.substring(authIndex, lineEnd);
  Serial.println("Auth line: " + authLine);
  
  // Find "Basic " part
  int basicIndex = authLine.indexOf("Basic ");
  if (basicIndex == -1) {
    Serial.println("No Basic auth found");
    return false;
  }
  
  // Extract base64 credentials
  String base64Creds = authLine.substring(basicIndex + 6);
  base64Creds.trim();
  Serial.println("Base64 creds: " + base64Creds);
  
  // Decode
  String decoded = base64_decode(base64Creds);
  Serial.println("Decoded: " + decoded);
  
  // Expected format: "username:password"
  String expected = String(authUsername) + ":" + String(authPassword);
  Serial.println("Expected: " + expected);
  
  return (decoded == expected);
}

/* ================= ASCON ================= */
uint8_t encryptionKey[16] = {
  0x00, 0x01, 0x02, 0x03,
  0x04, 0x05, 0x06, 0x07,
  0x08, 0x09, 0x0A, 0x0B,
  0x0C, 0x0D, 0x0E, 0x0F
};

uint8_t nonce[12] = {0};
Ascon128 cipher;

/* ================= MOTORS ================= */
int motor1Pin1 = 27;
int motor1Pin2 = 26;
int motor2Pin1 = 33;
int motor2Pin2 = 25;
int enable1Pin = 14;
int enable2Pin = 32;

const int freq = 30000;
const int resolution = 8;
int dutyCycle = 0;
String currentCommand = "STOP";

/* ================= HTTP ================= */
String header;
unsigned long currentTime = millis();
unsigned long previousTime = 0;
const long timeoutTime = 2000;

/* =====================================================
  client.println(".container { background: white; border-radius: 20px; box-shadow: 0 20px 60px rgba(0,0,0,0.3); padding: 40px; max-width: 500px; width: 100%; }");
  client.println("<div class='container'>");
  client.println("<h1>🤖 Secure Robot</h1>");
  client.println("<div class='subtitle'>🔒 Authenticated Control</div>");
  client.println("<div class='status-box'>");
  client.println("<div class='status-label'>Current Status</div>");
  client.println("<div class='status-value'>" + currentCommand + "</div>");
  client.println("</div>");
  client.println("<div class='button-grid'>");
  client.println("<button class='btn-forward' onclick=\"window.location='/cmd=forward'\">⬆️ Forward</button>");
  client.println("<button class='btn-left' onclick=\"window.location='/cmd=left'\">⬅️ Left</button>");
  client.println("<button class='btn-right' onclick=\"window.location='/cmd=right'\">➡️ Right</button>");
  client.println("<button class='btn-reverse' onclick=\"window.location='/cmd=reverse'\">⬇️ Reverse</button>");
  client.println("<button class='btn-stop' onclick=\"window.location='/cmd=stop'\">⏹️ Stop</button>");
  client.println("</div>");
  client.println("<div class='speed-section'>");
  client.println("<label class='speed-label'>Speed Control</label>");
  client.println("<input type='range' min='0' max='100' value='" + String(map(dutyCycle, 0, 255, 0, 100)) + "' onchange=\"window.location='/speed=' + this.value\">");
  client.println("<div class='speed-value'>" + String(map(dutyCycle, 0, 255, 0, 100)) + "%</div>");
  client.println("</div>");
  client.println("</div>");
  client.println("</body>");
  client.println("</html>");
}

/* =====================================================
   SEND UNAUTHORIZED PAGE
===================================================== */
void sendUnauthorized(WiFiClient &client) {
  client.println("HTTP/1.1 401 Unauthorized");
  client.println("WWW-Authenticate: Basic realm=\"Secure Robot Control\"");
  client.println("Content-type:text/html");
  client.println("Connection: close");
  client.println();
 
  client.println("<!DOCTYPE html>");
  client.println("<html>");
  client.println("<head><title>Access Denied</title></head>");
  client.println("<body style='font-family: Arial; text-align: center; padding: 50px;'>");
  client.println("<h1 style='color: #f44336;'>🔒 Access Denied</h1>");
  client.println("<p>Invalid credentials. Please login with:</p>");
  client.println("<p style='margin-top: 20px;'><strong>Username:</strong> admin<br><strong>Password:</strong> robot2024</p>");
  client.println("</body>");
  client.println("</html>");
}

/* =====================================================
   SETUP
===================================================== */
void setup() {
  Serial.begin(9600);
  delay(1000);
  
  Serial.println("\n=== SECURE ROBOT STARTING ===");

  pinMode(motor1Pin1, OUTPUT);
  pinMode(motor1Pin2, OUTPUT);
  pinMode(motor2Pin1, OUTPUT);
  pinMode(motor2Pin2, OUTPUT);

  ledcAttach(enable1Pin, freq, resolution);
  ledcAttach(enable2Pin, freq, resolution);

  ledcWrite(enable1Pin, 0);
  ledcWrite(enable2Pin, 0);

  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n✓ WiFi connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.println("\n=== AUTHENTICATION ===");
  Serial.println("Username: admin");
  Serial.println("Password: robot2024");
  Serial.println("\n=== SERVER READY ===\n");
  
  server.begin();
}

/* =====================================================
   LOOP
===================================================== */
void loop() {
  WiFiClient client = server.available();

  if (client) {
    currentTime = millis();
    previousTime = currentTime;
    String currentLine = "";
    header = "";
    bool authenticated = false;

    Serial.println("\n>>> New client connected");

    while (client.connected() && currentTime - previousTime <= timeoutTime) {
      currentTime = millis();

      if (client.available()) {
        char c = client.read();
        header += c;

        if (c == '\n') {
          if (currentLine.length() == 0) {
            // Headers complete, check authentication
            authenticated = checkAuthentication(header);
            
            Serial.println("Authentication result: " + String(authenticated ? "SUCCESS" : "FAILED"));

            if (!authenticated) {
              sendUnauthorized(client);
            } else {
              // Process commands
              if (header.indexOf("GET /cmd=") >= 0) {
                int p1 = header.indexOf("cmd=") + 4;
                int p2 = header.indexOf(" ", p1);
                String cmd = header.substring(p1, p2);
                controlMotor(cmd);
                Serial.println("✓ Command executed: " + cmd);
              }

              if (header.indexOf("GET /speed=") >= 0) {
                int p1 = header.indexOf("speed=") + 6;
                int p2 = header.indexOf(" ", p1);
                int speed = header.substring(p1, p2).toInt();
                dutyCycle = map(speed, 0, 100, 0, 255);
                ledcWrite(enable1Pin, dutyCycle);
                ledcWrite(enable2Pin, dutyCycle);
                Serial.println("✓ Speed set to: " + String(speed) + "%");
              }

              sendWebPage(client);
            }

            break;
          }
          currentLine = "";
        }
        else if (c != '\r') {
          currentLine += c;
        }
      }
    }

    header = "";
    client.stop();
    Serial.println(">>> Client disconnected\n");
  }
}