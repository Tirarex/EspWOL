#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include <WiFiUdp.h>
#include <WakeOnLan.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>

ESP8266WebServer server(80);
WiFiUDP UDP;
WakeOnLan WOL(UDP);

const char* pcFile = "/pc_list.json";
String Hostname = "wol";

// Структура для данных ПК
struct PC {
  String name;
  String mac;
  String ip;
};

// Вектор для хранения списка ПК
std::vector<PC> pcList;

// HTML-контент
const char* htmlPage = "<!DOCTYPE html>"
                       "<html lang='en'>"
                       "<head>"
                       "    <meta charset='UTF-8'>"
                       "    <meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                       "    <title>Wake on LAN</title>"
                       "    <link rel='stylesheet' href='https://maxcdn.bootstrapcdn.com/bootstrap/4.5.2/css/bootstrap.min.css'>"
                       "    <link rel='stylesheet' href='https://cdnjs.cloudflare.com/ajax/libs/font-awesome/5.15.4/css/all.min.css'>"
                       "    <script src='https://ajax.googleapis.com/ajax/libs/jquery/3.5.1/jquery.min.js'></script>"
                       "    <script src='https://maxcdn.bootstrapcdn.com/bootstrap/4.5.2/js/bootstrap.min.js'></script>"
                       "    <script>"
                       "        $(document).ready(function() {"
                       "            updatePCList();"
                       "        });"
                       "        function updatePCList() {"
                       "            fetch('/pc_list').then(response => {"
                       "                if (!response.ok) {"
                       "                    throw new Error('Network response was not ok');"
                       "                }"
                       "                return response.json();"
                       "            }).then(data => {"
                       "                if (!Array.isArray(data)) {"
                       "                    console.error('Expected an array but got:', data);"
                       "                    return;"
                       "                }"
                       "                const pcList = document.getElementById('pc-list');"
                       "                pcList.innerHTML = '';"
                       "                data.forEach((pc, index) => {"
                       "                    pcList.innerHTML += '<li class=\"list-group-item d-flex justify-content-between align-items-center\">' +"
                       "                        pc.name + ' - ' + pc.mac + ' - ' + pc.ip +"
                       "                        '<div class=\"ml-auto\">' +"
                       "                        '<button class=\"btn btn-warning btn-md mr-2\" onclick=\"editPC(' + index + ')\"><i class=\"fas fa-edit\"></i></button>' +"
                       "                        '<button class=\"btn btn-primary btn-md\" onclick=\"wakePC(\\'' + pc.mac + '\\')\"><i class=\"fas fa-play\"></i></button>' +"
                       "                        '</div></li>';"
                       "                });"
                       "            }).catch(error => {"
                       "                console.error('Fetch error:', error);"
                       "            });"
                       "        }"
                       "        function addPC() {"
                       "            const name = document.getElementById('pc-name').value;"
                       "            const mac = document.getElementById('pc-mac').value;"
                       "            const ip = document.getElementById('pc-ip').value;"
                       "            fetch('/add', {"
                       "                method: 'POST',"
                       "                headers: { 'Content-Type': 'application/json' },"
                       "                body: JSON.stringify({ name, mac, ip })"
                       "            }).then(response => {"
                       "                if (response.ok) {"
                       "                    updatePCList();"
                       "                    $('#add-pc-modal').modal('hide');"
                       "                    document.getElementById('pc-name').value = '';"
                       "                    document.getElementById('pc-mac').value = '';"
                       "                    document.getElementById('pc-ip').value = '';"
                       "                    showNotification('PC added successfully!', 'success');"
                       "                }"
                       "            });"
                       "        }"
                       "        function editPC(index) {"
                       "            fetch('/pc_list').then(response => response.json()).then(data => {"
                       "                const pc = data[index];"
                       "                document.getElementById('edit-pc-name').value = pc.name;"
                       "                document.getElementById('edit-pc-mac').value = pc.mac;"
                       "                document.getElementById('edit-pc-ip').value = pc.ip;"
                       "                $('#edit-pc-modal').data('index', index).modal('show');"
                       "            });"
                       "        }"
                       "        function saveEditPC() {"
                       "            const index = $('#edit-pc-modal').data('index');"
                       "            const name = document.getElementById('edit-pc-name').value;"
                       "            const mac = document.getElementById('edit-pc-mac').value;"
                       "            const ip = document.getElementById('edit-pc-ip').value;"
                       "            fetch('/edit', {"
                       "                method: 'POST',"
                       "                headers: { 'Content-Type': 'application/json' },"
                       "                body: JSON.stringify({ index, name, mac, ip })"
                       "            }).then(response => {"
                       "                if (response.ok) {"
                       "                    updatePCList();"
                       "                    $('#edit-pc-modal').modal('hide');"
                       "                    showNotification('PC updated successfully!', 'success');"
                       "                }"
                       "            });"
                       "        }"
                       "        function confirmDelete() {"
                       "            const index = $('#edit-pc-modal').data('index');"
                       "            fetch('/delete?index=' + index, { method: 'POST' }).then(response => {"
                       "                if (response.ok) {"
                       "                    updatePCList();"
                       "                    showNotification('PC deleted successfully!', 'danger');"
                       "                }"
                       "            });"
                       "            $('#edit-pc-modal').modal('hide');"
                       "        }"
                       "        function wakePC(mac) {"
                       "            fetch('/wake', {"
                       "                method: 'POST',"
                       "                headers: { 'Content-Type': 'application/json' },"
                       "                body: JSON.stringify({ mac })"
                       "            }).then(response => {"
                       "                if (response.ok) {"
                       "                    showNotification('WOL packet sent to ' + mac, 'info');"
                       "                }"
                       "            });"
                       "        }"
                       "        function showNotification(message, type) {"
                       "            const notification = $('<div class=\"alert alert-' + type + ' alert-dismissible fade show\" role=\"alert\">' + message + '<button type=\"button\" class=\"close\" data-dismiss=\"alert\" aria-label=\"Close\"><span aria-hidden=\"true\">&times;</span></button></div>');"
                       "            $('#notification-area').append(notification);"
                       "            setTimeout(() => { notification.alert('close'); }, 3000);"
                       "        }"
                       "        function resetWiFiSettings() {"
                       "            fetch('/reset_wifi', { method: 'POST' })"
                       "                .then(response => {"
                       "                    if (response.ok) {"
                       "                        showNotification('WiFi settings reset successfully!', 'success');"
                       "                        location.reload();"
                       "                    } else {"
                       "                        console.error('Failed to reset WiFi settings');"
                       "                        showNotification('Failed to reset WiFi settings.', 'danger');"
                       "                    }"
                       "                })"
                       "                .catch(error => console.error('Fetch error:', error));"
                       "        }"
                       "    </script>"
                       "</head>"
                       "<body class='bg-light text-dark'>"
                       "    <div class='container mt-5'>"
                       "        <h1 class='text-center'>Wake on LAN</h1>"
                       "        <div id='notification-area'></div>"
                       "        <h2 class='mt-4 d-flex justify-content-between align-items-center'>"
                       "            <span>PC List</span>"
                       "            <div class='ml-auto'>"
                       "             <button class='btn btn-success' data-toggle='modal' data-target='#add-pc-modal'>"
                       "             <i class='fas fa-plus'></i>"
                       "             </button>"
                       "             <button class='btn btn-secondary btn-md' title='Settings' data-toggle='modal' data-target='#reset-wifi-modal'>"
                       "             <i class='fas fa-cog'></i>"
                       "             </button>"
                       "            </div>"
                       "        </h2>"
                       "<ul id='pc-list' class='list-group mt-3'></ul>"
                       "    </div>"
                       "    <div class='modal fade' id='add-pc-modal' tabindex='-1' role='dialog' aria-labelledby='addPCLabel' aria-hidden='true'>"
                       "        <div class='modal-dialog' role='document'>"
                       "            <div class='modal-content'>"
                       "                <div class='modal-header'>"
                       "                    <h5 class='modal-title' id='addPCLabel'>Add PC</h5>"
                       "                    <button type='button' class='close' data-dismiss='modal' aria-label='Close'>"
                       "                        <span aria-hidden='true'>&times;</span>"
                       "                    </button>"
                       "                </div>"
                       "                <div class='modal-body'>"
                       "                    <div class='form-group'>"
                       "                        <label for='pc-name'>Name</label>"
                       "                        <input type='text' class='form-control' id='pc-name' placeholder='Enter PC name'>"
                       "                    </div>"
                       "                    <div class='form-group'>"
                       "                        <label for='pc-mac'>MAC Address</label>"
                       "                        <input type='text' class='form-control' id='pc-mac' placeholder='Enter MAC address'>"
                       "                    </div>"
                       "                    <div class='form-group'>"
                       "                        <label for='pc-ip'>IP Address</label>"
                       "                        <input type='text' class='form-control' id='pc-ip' placeholder='Enter IP address'>"
                       "                    </div>"
                       "                </div>"
                       "                <div class='modal-footer'>"
                       "                    <button type='button' class='btn btn-primary' onclick='addPC()'>Add</button>"
                       "                    <button type='button' class='btn btn-secondary' data-dismiss='modal'>Close</button>"
                       "                </div>"
                       "            </div>"
                       "        </div>"
                       "    </div>"
                       "    <div class='modal fade' id='edit-pc-modal' tabindex='-1' role='dialog' aria-labelledby='editPCLabel' aria-hidden='true'>"
                       "        <div class='modal-dialog' role='document'>"
                       "            <div class='modal-content'>"
                       "                <div class='modal-header'>"
                       "                    <h5 class='modal-title' id='editPCLabel'>Edit PC</h5>"
                       "                    <button type='button' class='close' data-dismiss='modal' aria-label='Close'>"
                       "                        <span aria-hidden='true'>&times;</span>"
                       "                    </button>"
                       "                </div>"
                       "                <div class='modal-body'>"
                       "                    <div class='form-group'>"
                       "                        <label for='edit-pc-name'>Name</label>"
                       "                        <input type='text' class='form-control' id='edit-pc-name' placeholder='Enter PC name'>"
                       "                    </div>"
                       "                    <div class='form-group'>"
                       "                        <label for='edit-pc-mac'>MAC Address</label>"
                       "                        <input type='text' class='form-control' id='edit-pc-mac' placeholder='Enter MAC address'>"
                       "                    </div>"
                       "                    <div class='form-group'>"
                       "                        <label for='edit-pc-ip'>IP Address</label>"
                       "                        <input type='text' class='form-control' id='edit-pc-ip' placeholder='Enter IP address'>"
                       "                    </div>"
                       "                </div>"
                       "                <div class='modal-footer'>"
                       "                    <button type='button' class='btn btn-primary' onclick='saveEditPC()'>Save changes</button>"
                       "                    <button type='button' class='btn btn-danger' onclick='confirmDelete()'>Delete</button>"
                       "                    <button type='button' class='btn btn-secondary' data-dismiss='modal'>Close</button>"
                       "                </div>"
                       "            </div>"
                       "        </div>"
                       "    </div>"
                       "    <div class='modal fade' id='reset-wifi-modal' tabindex='-1' role='dialog' aria-labelledby='resetWiFiLabel' aria-hidden='true'>"
                       "        <div class='modal-dialog' role='document'>"
                       "            <div class='modal-content'>"
                       "                <div class='modal-header'>"
                       "                    <h5 class='modal-title' id='resetWiFiLabel'>Reset WiFi Settings</h5>"
                       "                    <button type='button' class='close' data-dismiss='modal' aria-label='Close'>"
                       "                        <span aria-hidden='true'>&times;</span>"
                       "                    </button>"
                       "                </div>"
                       "                <div class='modal-body'>"
                       "                    <p>Are you sure you want to reset WiFi settings? This will clear saved credentials.</p>"
                       "                </div>"
                       "                <div class='modal-footer'>"
                       "                    <button type='button' class='btn btn-danger' onclick='resetWiFiSettings()'>Reset</button>"
                       "                    <button type='button' class='btn btn-secondary' data-dismiss='modal'>Cancel</button>"
                       "                </div>"
                       "            </div>"
                       "        </div>"
                       "    </div>"
                       "    <footer class='text-center mt-5'>"
                       "        <p>&copy; 2024 Tirarex</p>"
                       "    </footer>"
                       "</body>"
                       "</html>";


// Функция для загрузки данных ПК из JSON-файла
void loadPCData() {
  if (LittleFS.begin()) {
    File file = LittleFS.open(pcFile, "r");
    if (file) {
      StaticJsonDocument<1024> doc;
      DeserializationError error = deserializeJson(doc, file);
      if (!error) {
        pcList.clear();  // Очистить существующий список перед загрузкой новых данных
        for (JsonVariant v : doc.as<JsonArray>()) {
          PC pc;
          pc.name = v["name"].as<String>();
          pc.mac = v["mac"].as<String>();
          pc.ip = v["ip"].as<String>();
          pcList.push_back(pc);
        }
      }
      file.close();
    }
    LittleFS.end();
  }
}

// Функция для сохранения данных ПК в JSON-файл
void savePCData() {
  if (LittleFS.begin()) {
    File file = LittleFS.open(pcFile, "w");
    if (file) {
      StaticJsonDocument<1024> doc;
      JsonArray array = doc.to<JsonArray>();
      for (const PC& pc : pcList) {
        JsonObject obj = array.createNestedObject();
        obj["name"] = pc.name;
        obj["mac"] = pc.mac;
        obj["ip"] = pc.ip;  // Сохранить IP
      }
      serializeJson(doc, file);
      file.close();
    }
    LittleFS.end();
  }
}

// Функция для обработки корневого запроса
void handleRoot() {
  server.send(200, "text/html", htmlPage);
}

// Функция для обработки запроса списка ПК
void handlePCList() {
  String jsonResponse;
  StaticJsonDocument<1024> doc;
  JsonArray array = doc.to<JsonArray>();
  for (const PC& pc : pcList) {
    JsonObject obj = array.createNestedObject();
    obj["name"] = pc.name;
    obj["mac"] = pc.mac;
    obj["ip"] = pc.ip;  
  }
  serializeJson(doc, jsonResponse);
  server.send(200, "application/json", jsonResponse);
}

// Функция для добавления нового ПК
void handleAddPC() {
  if (server.method() == HTTP_POST) {
    String body = server.arg("plain");
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, body);
    PC pc;
    pc.name = doc["name"].as<String>();
    pc.mac = doc["mac"].as<String>();
    pc.ip = doc["ip"].as<String>();  
    pcList.push_back(pc);
    savePCData();  
    server.send(200, "text/plain", "PC added");
  } else {
    server.send(405, "text/plain", "Method Not Allowed");
  }
}

// Функция для редактирования ПК
void handleEditPC() {
  if (server.method() == HTTP_POST) {
    String body = server.arg("plain");
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, body);
    int index = doc["index"];
    if (index >= 0 && index < pcList.size()) {
      PC& pc = pcList[index];
      pc.name = doc["name"].as<String>();
      pc.mac = doc["mac"].as<String>();
      pc.ip = doc["ip"].as<String>();  
      savePCData();                   
      server.send(200, "text/plain", "PC updated");
    } else {
      server.send(404, "text/plain", "PC not found");
    }
  } else {
    server.send(405, "text/plain", "Method Not Allowed");
  }
}

// Функция для удаления ПК
void handleDeletePC() {
  if (server.method() == HTTP_POST) {
    int index = server.arg("index").toInt();
    if (index >= 0 && index < pcList.size()) {
      pcList.erase(pcList.begin() + index);
      savePCData(); 
      server.send(200, "text/plain", "PC deleted");
    } else {
      server.send(404, "text/plain", "PC not found");
    }
  } else {
    server.send(405, "text/plain", "Method Not Allowed");
  }
}

// Функция для пробуждения ПК
void handleWakePC() {
  if (server.method() == HTTP_POST) {
    String body = server.arg("plain");
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, body);
    String mac = doc["mac"].as<String>();
    if (WOL.sendMagicPacket(mac.c_str())) {
      server.send(200, "text/plain", "WOL packet sent");
    } else {
      server.send(400, "text/plain", "Failed to send WOL packet");
    }
  } else {
    server.send(405, "text/plain", "Method Not Allowed");
  }
}

// Функция для сброса настроек Wi-Fi
void resetWiFiSettings() {
  WiFiManager wifiManager;
  wifiManager.resetSettings();  // Сбросить настройки WiFi
}

// Настройка сервера
void setup() {
  Serial.begin(115200);
  WiFi.hostname(Hostname.c_str());
  LittleFS.begin();
  loadPCData();  // Загрузить данные ПК при старте
  WiFiManager wifiManager;
  wifiManager.autoConnect("WOL-ESP8266");  // Авто подключение
  server.on("/", handleRoot);
  server.on("/pc_list", handlePCList);
  server.on("/add", handleAddPC);
  server.on("/edit", handleEditPC);
  server.on("/delete", handleDeletePC);
  server.on("/wake", handleWakePC);
  server.on("/reset_wifi", []() {
    resetWiFiSettings();
    server.send(200, "text/plain", "WiFi settings reset");
  });
  server.begin();
}

void loop() {
  server.handleClient();
  delay(1); // Уменьшим потребление на 60% добавив делей https://hackaday.com/2022/10/28/esp8266-web-server-saves-60-power-with-a-1-ms-delay/
}
