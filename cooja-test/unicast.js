packetsReceived= new Array();
packetsSent = new Array();
totalReceived = 0;
totalSent = 0;
nodeCount = 2;

TIMEOUT(30000, log.log("Total packets received:" + totalReceived + ", total packets sent:" + totalSent+ "\n"));

for(i = 0; i <= nodeCount; i++) {
	packetsReceived[i] = 0;
	packetsSent[i] = 0;
}
while(1) {
	YIELD();
	msgArray = msg.split(' ');
	if(msgArray[0].equals("DATA")) 
    {
        if(msgArray.length == 4) 
        {
            // packet received
            packetsReceived[id]++; 
	        totalReceived++;
	    }		
        else if(msgArray.length == 2) 
        {			
            // packet sent
            packetsSent[id]++; 
	        totalSent++;
        }
        log.log("MoteID: " + id + "; r/s:" + packetsReceived[id] + "/" + packetsSent[id] + "\n");
    }
}