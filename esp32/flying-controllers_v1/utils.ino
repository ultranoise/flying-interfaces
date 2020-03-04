
//////////////////////////////////////////////
// Aux Functions (converters, parsers, smoothing, filters, etc)
//
/////////////////////////////////////////////

//function to convert a string to an IPAdress type
IPAddress string2IP(String strIP){
    
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
    
    IPAddress ipo( Parts[0], Parts[1], Parts[2], Parts[3] );
    return ipo;
}

//function to convert IPAdress type to string
String IpAddress2String(const IPAddress& ipAddress){
  return String(ipAddress[0]) + String(".") +\
  String(ipAddress[1]) + String(".") +\
  String(ipAddress[2]) + String(".") +\
  String(ipAddress[3])  ;
}
