

/*

Processing sketch:

// import UDP library
import hypermedia.net.*;


UDP udp;  // define the UDP object

int compte=0;
//  init

void setup() {

  // create a new datagram connection on port 6000
  udp = new UDP( this, 6000 );
  //udp.log( true );     // <-- printout the connection activity
  // and wait for incomming message
  udp.listen( true );
}

//process events
void draw() {
  ;
}

void receive( byte[] data ) {       // <-- default handler
  //void receive( byte[] data, String ip, int port ) {  // <-- extended handler

  compte++; 
  // print the result
  println( compte);
}

 
 */
