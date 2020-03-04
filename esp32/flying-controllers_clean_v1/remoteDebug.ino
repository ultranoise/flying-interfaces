
void configureDebugService(){
    String hostNameWifi = HOST_NAME;
    hostNameWifi.concat(".local");   //it should be read from configurations settings

    #ifdef ESP8266 // Only for it
        WiFi.hostname(hostNameWifi);
    #endif
    
    #ifdef USE_MDNS  // Use the MDNS ?
    
        if (MDNS.begin(HOST_NAME)) {
            Serial.print("* MDNS responder started. Hostname -> ");
            Serial.println(HOST_NAME);
        }
    
        MDNS.addService("telnet", "tcp", 23);
    
    #endif
  
  // Initialize RemoteDebug
  Debug.begin(HOST_NAME); // Initialize the WiFi server

  Debug.setResetCmdEnabled(true); // Enable the reset command

  Debug.showProfiler(true); // Profiler (Good to measure times, to optimize codes)
  Debug.showColors(true); // Colors
}
