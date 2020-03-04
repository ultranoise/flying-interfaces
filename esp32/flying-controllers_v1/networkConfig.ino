

//////////////////////////////////////////////////////////////
// Network functions (Wifi connect and handle connections)
//
//////////////////////////////////////////////////////////////


// basic Access Point creation
void createAccessPoint(){
   
   //register to wifi event handler
    if(!registered){
      registered = true;
      WiFi.onEvent(WiFiEvent);
    }
 
    WiFi.mode(WIFI_AP);
    WiFi.softAP(APssid);
    //Serial.println("Wait 100 ms for AP_START...");
    delay(100);  //"Wait 100 ms for AP_START..."

    Serial.println("Set softAPConfig");
    IPAddress Ip(192, 168, 0, 1);       //We fix an IP easy to recover without serial monitor
    IPAddress NMask(255, 255, 255, 0);
    WiFi.softAPConfig(Ip, Ip, NMask);
  
    IPAddress myIP1 = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(myIP1);
    APconnected = true;
}

    
/////////////////////////////////////////////////////////////
//wifi basic STA connection function
void connectToWiFi(String ssid, String pwd){
  
  //register event handler. This allows receiving messages of connection, disconnections, etc
  if(!registered){
    registered = true;
    WiFi.onEvent(WiFiEvent);
  }
  
  // delete old config if already connected
  WiFi.disconnect(true);
  
  //we will simply use gateway as DNS address
  primaryDNS = staGateway;
  secondaryDNS = staGateway;
  
  
  Serial.print("connecting to: ");
  Serial.println(ssid);

  //connect to wifi
  WiFi.begin(ssid.c_str(), pwd.c_str());  //I use c_str() as we have to convert strings to arrays of characters
  //Comment below for DHCP
  WiFi.config(staIP, staGateway, staSubnet, primaryDNS,secondaryDNS);   //fix IP at network

  //while (WiFi.status() != WL_CONNECTED) { //wait until connected
  //      delay(500);
  //      Serial.print(".");
  //  }
    
  delay(1000);
  //Some info
  Serial.println("");
  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("ESP Mac Address: ");
  Serial.println(WiFi.macAddress());
  Serial.print("Subnet Mask: ");
  Serial.println(WiFi.subnetMask());
  Serial.print("Gateway IP: ");
  Serial.println(WiFi.gatewayIP());
  Serial.print("DNS: ");
  Serial.println(WiFi.dnsIP());

}

/////////////////////////////////////////////////////////////
// wifi event handler
// These are all the messages we receive from Wifi Manager
void WiFiEvent(WiFiEvent_t event){
    Serial.printf("[WiFi-event] event: %d\n", event);
    
    switch(event) {
       case SYSTEM_EVENT_STA_GOT_IP:    //WE HAVE GOT AN IP!!
          //When connected set 
          Serial.print("WiFi connected! IP address: ");

          //initialize UDP state when we get an IP
          //This initializes the transfer buffer
          udp.begin(WiFi.localIP(),udpPort);
          connected = true; 
          ok = true;
          break;
          
       case SYSTEM_EVENT_STA_DISCONNECTED:  //try to reconnect if disconnected
          //Serial.println("WiFi lost connection");
          Serial.println("Disconnected from WiFi access point");
          connected = false;
          
          WiFi.disconnect(true);
          ok = false;
          //Initiate connection
          connectToWiFi(network, password);
          delay(1000);    
          break;
      
        case SYSTEM_EVENT_WIFI_READY: 
            Serial.println("WiFi interface ready");
            break;
        case SYSTEM_EVENT_SCAN_DONE:
            Serial.println("Completed scan for access points");
            break;
        case SYSTEM_EVENT_STA_START:
            Serial.println("WiFi client started");
            break;
        case SYSTEM_EVENT_STA_STOP:
            Serial.println("WiFi clients stopped");
            break;
        case SYSTEM_EVENT_STA_CONNECTED:
            Serial.println("Connected to STA");
            break;
        case SYSTEM_EVENT_STA_AUTHMODE_CHANGE:
            Serial.println("Authentication mode of access point has changed");
            break;
        case SYSTEM_EVENT_STA_LOST_IP:
            Serial.println("Lost IP address and IP address is reset to 0");
            break;
        case SYSTEM_EVENT_STA_WPS_ER_SUCCESS:
            Serial.println("WiFi Protected Setup (WPS): succeeded in enrollee mode");
            break;
        case SYSTEM_EVENT_STA_WPS_ER_FAILED:
            Serial.println("WiFi Protected Setup (WPS): failed in enrollee mode");
            break;
        case SYSTEM_EVENT_STA_WPS_ER_TIMEOUT:
            Serial.println("WiFi Protected Setup (WPS): timeout in enrollee mode");
            break;
        case SYSTEM_EVENT_STA_WPS_ER_PIN:
            Serial.println("WiFi Protected Setup (WPS): pin code in enrollee mode");
            break;
        case SYSTEM_EVENT_AP_START:
            Serial.println("WiFi access point started");
            APconnected = true;
            connected = true; 
            updateStations(); 
            break;
        case SYSTEM_EVENT_AP_STOP:
            Serial.println("WiFi access point  stopped");
            break;
        case SYSTEM_EVENT_AP_STACONNECTED:
            Serial.println("Client connected");
            connected = true; 
            break;
        case SYSTEM_EVENT_AP_STADISCONNECTED:
            Serial.println("Client disconnected");
            updateStations(); 
            break;
        case SYSTEM_EVENT_AP_STAIPASSIGNED:
            Serial.println("Assigned IP address to client");
            updateStations(); 
            connected = true; 
            break;
        case SYSTEM_EVENT_AP_PROBEREQRECVED:
            Serial.println("Received probe request");
            break;
        case SYSTEM_EVENT_GOT_IP6:
            Serial.println("IPv6 is preferred");
            break;
        case SYSTEM_EVENT_ETH_START:
            Serial.println("Ethernet started");
            break;
        case SYSTEM_EVENT_ETH_STOP:
            Serial.println("Ethernet stopped");
            break;
        case SYSTEM_EVENT_ETH_CONNECTED:
            Serial.println("Ethernet connected");
            break;
        case SYSTEM_EVENT_ETH_DISCONNECTED:
            Serial.println("Ethernet disconnected");
            break;
        case SYSTEM_EVENT_ETH_GOT_IP:
            Serial.println("Obtained IP address");
        break;
    }
}

///////////////////////////////////////////////////////////////////////
// Handling information about connected clients in access point mode
// For sending OSC data to clients we need to know their IPs and this
// function makes the work.
void updateStations() {    
  
  wifi_sta_list_t stationList;
  esp_wifi_ap_get_sta_list(&stationList);  

  //update number of clients (stations connected)
  numClients = stationList.num;
  
  Serial.println("-----------------");
  Serial.print("Number of connected stations: ");
  Serial.println(stationList.num);
  Serial.println("-----------------");

  //Now obtain their Mac addresses and IP addresses
  tcpip_adapter_sta_list_t adapter_sta_list;
  memset(&adapter_sta_list, 0, sizeof(adapter_sta_list));
  
  ESP_ERROR_CHECK(tcpip_adapter_get_sta_list(&stationList, &adapter_sta_list));

  //we have to convert station information to IPAdress type of arduino
  for(int i = 0; i < adapter_sta_list.num; i++) {
    tcpip_adapter_sta_info_t station = adapter_sta_list.sta[i];
    printf("%d - mac: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x - IP: %s\n", i + 1,
    station.mac[0], station.mac[1], station.mac[2],
    station.mac[3], station.mac[4], station.mac[5],
    ip4addr_ntoa(&(station.ip)));
    
    String ipi = ip4addr_ntoa(&(station.ip));
    String strIP = ipi; 
    
    int Parts[4] = {0,0,0,0};
    int Part = 0;
    for ( int i=0; i<strIP.length(); i++ ) {
      char c = strIP[i];
      if ( c == '.' ) {
        Part++;
        continue;
      }
      Parts[Part] *= 10;
      Parts[Part] += c - '0';
    }

    //finally this is the array of clients ip addresses
    IPAddress ipo( Parts[0], Parts[1], Parts[2], Parts[3] );
    clientsAddress[i]= ipo;
 
  }

}

void discoverMDNShosts(){
  //setup mDNS for collecting the IPs of other machines in the network, only in STA Mode
  if(accesspoint == false){
    delay(2000);
    if (!MDNS.begin("C2_MDNS")) {
          Serial.println("Error setting up MDNS responder!");
         while(1){
              delay(1000);
          }
      }
  
    //browse samba services to discover available devices
    browseService("smb", "tcp");
    delay(100);
    
      //browseService("printer", "tcp");
      //delay(100);
      //browseService("workstation", "tcp");
      //delay(100);
    
  }
    
}


void browseService(const char * service, const char * proto){
    Serial.println(" ");
    Serial.printf("Browsing for service _%s._%s.local. ... ", service, proto);
    nServices = MDNS.queryService(service, proto);
    if (nServices == 0) {
        Serial.println("no services found");
    } else {
        Serial.print(nServices);
        Serial.println(" service(s) found");
        for (int i = 0; i < nServices; ++i) {
            // Print details for each service found
            Serial.print("  ");
            Serial.print(i + 1);
            Serial.print(": ");
            Serial.print(MDNS.hostname(i));
            Serial.print(" (");
            Serial.print(MDNS.IP(i));
            Serial.print(":");
            Serial.print(MDNS.port(i));
            Serial.println(")");
        }
    }
    Serial.println();
}
