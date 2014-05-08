TIMEOUT(1200000, log.log("Total PRR " + totalPRR + "= recv " + totalReceived + "+ sent " + totalSent +"\n" + "AHC" + hopsCount/totalInHopsReceived + "THC " + hopsCount + "TRN " + totalInHopsReceived + "\n"));
packetsReceived= new Array();
packetsSent = new Array();
nodeCount = 100;
totalPRR = 0;
delayTime = 0;
hopsCount = 0;
totalInDelayReceived = 0;
totalInHopsReceived = 0;

for(i = 0; i <= nodeCount; i++) {
    packetsReceived[i] = 0;
    packetsSent[i] = 0;
}

while(1) {
    YIELD();

    msgArray = msg.split(' ');
    if(msgArray[0].equals("DATA")) {
        log.log(time + ":" + id + ":" + msg + "\n");
		if(msgArray.length == 4) {
	    	// Received packet
	    	senderID = parseInt(msgArray[3]);
	    	packetsReceived[senderID]++;
			totalReceived = totalSent = 0;
			for(i = 1; i <= nodeCount; i++) {
				totalReceived += packetsReceived[i];
				totalSent += packetsSent[i];
			}
			totalPRR = totalReceived / totalSent;
//			log.log("Total PRR " + totalPRR + 
//				  "= recv " + totalReceived + "+ sent " + totalSent + "\n");
		} else if(msgArray.length == 10) {
			// Sent packet
			sinkID = parseInt(msgArray[5]);
			packetsSent[sinkID]++;
	    }
	} else if(msgArray[0].equals("HOPS")) {
		hopsCount += parseInt(msgArray[1]);
		totalInHopsReceived++;
//		log.log("THC " + hopsCount + " TRN " + totalInHopsReceived +
//				" AHC " + hopsCount/totalInHopsReceived + "\n");
	}

}

