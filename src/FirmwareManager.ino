/* 
FSWebServer - Example WebServer with SPIFFS backend for esp8266
Copyright (c) 2015 Hristo Gochkov. All rights reserved.
This file is part of the ESP8266WebServer library for Arduino environment.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.
This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.
You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <FS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <SPIFlash.h>

#define CS 16
#define DBG_OUTPUT_PORT Serial
#define PAGES 8192 // 8192 pages x 256 bytes = 2MB = 16MBit

char ssid[64];
char password[64];
char otaPassword[64]; 
const char* host = "dc-firmware-manager";
const char* WiFiAPPSK = "geheim1234";
IPAddress ipAddress( 192, 168, 4, 1 );
bool inInitialSetupMode = false;

String debug;

const uint8_t empty_buffer[256] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

ESP8266WebServer server(80);
SPIFlash flash(CS);
//holds the current upload
File fsUploadFile;

String getContentType(String filename){
    if(server.hasArg("download")) return "application/octet-stream";
    else if(filename.endsWith(".htm")) return "text/html";
    else if(filename.endsWith(".html")) return "text/html";
    else if(filename.endsWith(".css")) return "text/css";
    else if(filename.endsWith(".js")) return "application/javascript";
    else if(filename.endsWith(".png")) return "image/png";
    else if(filename.endsWith(".gif")) return "image/gif";
    else if(filename.endsWith(".jpg")) return "image/jpeg";
    else if(filename.endsWith(".ico")) return "image/x-icon";
    else if(filename.endsWith(".xml")) return "text/xml";
    else if(filename.endsWith(".pdf")) return "application/x-pdf";
    else if(filename.endsWith(".zip")) return "application/x-zip";
    else if(filename.endsWith(".gz")) return "application/x-gzip";
    return "text/plain";
}

bool handleFileRead(String path){
    if (path.endsWith("/")) {
        path += "index.htm";
    }
    String contentType = getContentType(path);
    String pathWithGz = path + ".gz";
    if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) {
        if (SPIFFS.exists(pathWithGz)) {
            path += ".gz";
        }
        File file = SPIFFS.open(path, "r");
        size_t sent = server.streamFile(file, contentType);
        file.close();
        return true;
    }
    return false;
}

/*
    curl -D - -F "file=@$PWD/output_file.rbf" "http://dc-firmware-manager.local/upload-firmware?size=368011"
*/
void handleFileUpload(){
    HTTPUpload& upload = server.upload();
    int totalSize = server.arg("size").toInt();

    if (upload.status == UPLOAD_FILE_START) {
        fsUploadFile = SPIFFS.open("/flash.bin", "w");
        server.sendContent("Receiving flash.bin:\n");
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (fsUploadFile) {
            fsUploadFile.write(upload.buf, upload.currentSize);
            server.sendContent(String(upload.totalSize) + "/" + totalSize + "\n");
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (fsUploadFile) {
            fsUploadFile.close();
            server.sendContent(String(upload.totalSize) + "/" + totalSize + "\n");
            server.sendContent(""); // *** END 1/2 ***
            server.client().stop(); // *** END 2/2 ***    
        }
    }
}

void _readCredentials(const char *filename, char *target) {
    bool exists = SPIFFS.exists(filename);
    if (exists) {
        File f = SPIFFS.open(filename, "r");
        if (f) {
            f.readString().toCharArray(target, 64);
            f.close();
        }
    }
}

void setupCredentials(void) {
    DBG_OUTPUT_PORT.printf("Reading stored passwords...\n");
    _readCredentials("/ssid.txt", ssid);
    _readCredentials("/password.txt", password);
    _readCredentials("/ota-pass.txt", otaPassword);
}

void setupAPMode(void) {
    WiFi.mode(WIFI_AP);
    uint8_t mac[WL_MAC_ADDR_LENGTH];
    WiFi.softAPmacAddress(mac);
    String macID = String(mac[WL_MAC_ADDR_LENGTH - 2], HEX) +
    String(mac[WL_MAC_ADDR_LENGTH - 1], HEX);
    macID.toUpperCase();
    String AP_NameString = String(host) + String("-") + macID;
    
    char AP_NameChar[AP_NameString.length() + 1];
    memset(AP_NameChar, 0, AP_NameString.length() + 1);
    
    for (int i=0; i<AP_NameString.length(); i++)
    AP_NameChar[i] = AP_NameString.charAt(i);
    
    WiFi.softAP(AP_NameChar, WiFiAPPSK);
    DBG_OUTPUT_PORT.print("SSID: ");
    DBG_OUTPUT_PORT.println(AP_NameChar);
    DBG_OUTPUT_PORT.print("APPSK: ");
    DBG_OUTPUT_PORT.println(WiFiAPPSK);
    inInitialSetupMode = true;
}

bool copyBlockwise(String source, String destination, unsigned int length) {
    unsigned int i = 0;
    uint8_t buff[256];
    File src = SPIFFS.open(source, "r");
    if (src) {
        File dest = SPIFFS.open(destination, "w");
        if (dest) {
            server.sendContent("shorten file:\n");
            while (i <= length) {
                server.sendContent(String(i) + "\n");
                src.readBytes((char *) buff, 256);
                dest.write(buff, 256);
                i++;
            }
            dest.close();
        }
        src.close();
    }
}

void initBuffer(uint8_t *buffer) {
    for (int i = 0 ; i < 256 ; i++) { 
        buffer[i] = 0xff; 
        yield(); 
    }
}

void handleFileList() {
    if (!server.hasArg("dir")) {
        server.send(500, "text/plain", "BAD ARGS"); 
        return;
    }
    
    String path = server.arg("dir");
    DBG_OUTPUT_PORT.println("handleFileList: " + path);
    Dir dir = SPIFFS.openDir(path);
    path = String();
    
    String output = "[";
    while(dir.next()){
        File entry = dir.openFile("r");
        if (output != "[") output += ',';
        bool isDir = false;
        output += "{\"type\":\"";
        output += (isDir)?"dir":"file";
        output += "\",\"name\":\"";
        output += String(entry.name()).substring(1);
        output += "\",\"length\":\"";
        output += String(entry.size());
        output += "\"}";
        entry.close();
    }
    
    output += "]";
    server.send(200, "text/json", output);
}

void handleProgramFlash() {
    unsigned int page = 0;
    uint8_t buffer[256];
    // initialze
    initBuffer(buffer);
    
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "-1");
    server.setContentLength(CONTENT_LENGTH_UNKNOWN); // *** BEGIN ***
    server.send(200, "text/plain", "");
    
    File f = SPIFFS.open("/flash.bin", "r");
    int pagesToProgram = f.size() / 256;
    if (f) {
        flash.enable();
        flash.chip_erase();
        while (f.readBytes((char *) buffer, 256) && page < PAGES) {
            server.sendContent(String(page) + "/" + pagesToProgram + "\n");
            flash.page_write(page, buffer);
            // cleanup buffer
            initBuffer(buffer);
            page++;
        }
        flash.disable();
        f.close();
    }
    
    server.sendContent(""); // *** END 1/2 ***
    server.client().stop(); // *** END 2/2 ***
}

void handleDownloadFirmware() {
    uint8_t page_buffer[256];
    unsigned int last_page = 0;
    WiFiClient client = server.client();

    client.print("HTTP/1.1 200 OK\r\n");
    client.print("Content-Disposition: attachment; filename=flash.raw.bin\r\n");
    client.print("Content-Type: application/octet-stream\r\n");
    client.print("Content-Length: -1\r\n");
    client.print("Connection: close\r\n");
    client.print("Access-Control-Allow-Origin: *\r\n");
    client.print("\r\n");

    flash.enable();
    for (unsigned int i = 0; i < PAGES; ++i) {
        flash.page_read(i, page_buffer);
        client.write((const char*) page_buffer, 256);
    }
    flash.disable();
    client.stop();
}

void setupArduinoOTA() {
    DBG_OUTPUT_PORT.printf("Setting up ArduinoOTA...\n");
    
    ArduinoOTA.setPort(8266);
    ArduinoOTA.setHostname(host);
    if (strlen(otaPassword)) {
        ArduinoOTA.setPassword(otaPassword);
    }
    
    ArduinoOTA.onStart([]() {
        Serial.println("Start");
    });
    ArduinoOTA.onEnd([]() {
        Serial.println("\nEnd");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });
    ArduinoOTA.begin();
    Serial.println("Ready");
    Serial.print("IP address: ");
    Serial.println(ipAddress);
}

void setupWiFi() {
    //WIFI INIT
    WiFi.softAPdisconnect(true);
    WiFi.setAutoConnect(true);
    WiFi.setAutoReconnect(true);
    WiFi.mode(WIFI_STA);
    
    debug += "WiFi.getAutoConnect: " + String(WiFi.getAutoConnect()) + "\n";

    DBG_OUTPUT_PORT.printf("Connecting to %s\n", ssid);
    if (String(WiFi.SSID()) != String(ssid)) {
        WiFi.begin(ssid, password);
        debug += "WiFi.begin: " + String(password) + "@" + String(ssid) + "\n";
    }
    
    bool success = true;
    int tries = 0;
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        DBG_OUTPUT_PORT.print(".");
        if (tries == 60) {
            WiFi.disconnect();
            success = false;
            break;
        }
        tries++;
        debug += "." + String(tries);
    }
    debug += "success: " + String(success) + "\n";
    if (!success) {
        // setup AP mode to configure ssid and password
        setupAPMode();
    } else {
        ipAddress = WiFi.localIP();
        DBG_OUTPUT_PORT.println("");
        DBG_OUTPUT_PORT.print("Connected! IP address: ");
        DBG_OUTPUT_PORT.println(ipAddress);
    }
    
    if (MDNS.begin(host, ipAddress)) {
        DBG_OUTPUT_PORT.println("mDNS started");
        MDNS.addService("http", "tcp", 80);
        DBG_OUTPUT_PORT.print("Open http://");
        DBG_OUTPUT_PORT.print(host);
        DBG_OUTPUT_PORT.println(".local/edit to see the file browser");
    }
}

void setupHTTPServer() {
    DBG_OUTPUT_PORT.printf("Setting up HTTP server...\n");

    server.on("/list", HTTP_GET, handleFileList);
    server.on("/flash-firmware", HTTP_GET, handleProgramFlash);
    server.on("/download-firmware", HTTP_GET, handleDownloadFirmware);
    server.on("/delete-downloaded-firmware", HTTP_GET, [](){ 
        SPIFFS.remove("/flash.raw.bin");
    });
    server.on("/upload-firmware", HTTP_POST, [](){ 
        server.setContentLength(CONTENT_LENGTH_UNKNOWN); // *** BEGIN ***
        server.send(200, "text/plain", ""); 
    }, handleFileUpload);
    
    server.on("/debug", HTTP_GET, [](){
        server.send(200, "text/plain", 
              debug + "\n"
            + "[" + String(ssid) + "]\n"
            + "[" + String(password) + "]\n"
            + "[" + String(otaPassword) + "]\n"
            + "[" + String(ipAddress) + "]\n"
        );
        WiFi.printDiag(DBG_OUTPUT_PORT);
    });
    server.onNotFound([](){
        if (!handleFileRead(server.uri())) {
            server.send(404, "text/plain", "FileNotFound");
        }
    });
    server.begin();
}

void setupSPIFFS() {
    DBG_OUTPUT_PORT.printf("Setting up SPIFFS...\n");
    SPIFFS.begin();
    {
        Dir dir = SPIFFS.openDir("/");
        while (dir.next()) {    
            String fileName = dir.fileName();
            size_t fileSize = dir.fileSize();
            DBG_OUTPUT_PORT.printf("FS File: %s, size: %lu\n", fileName.c_str(), fileSize);
        }
        DBG_OUTPUT_PORT.printf("\n");
    }
}

void setupDebug() {
    DBG_OUTPUT_PORT.begin(115200);
    DBG_OUTPUT_PORT.print("\n");
    DBG_OUTPUT_PORT.print("FirmwareManager starting...\n");
    DBG_OUTPUT_PORT.setDebugOutput(true);
}

void setup(void){

    setupDebug();
    setupSPIFFS();
    setupCredentials();
    setupWiFi();
    setupHTTPServer();
    
    if (inInitialSetupMode || strlen(otaPassword)) {
        setupArduinoOTA();
    }
}

void loop(void){
    server.handleClient();
    ArduinoOTA.handle();
}
