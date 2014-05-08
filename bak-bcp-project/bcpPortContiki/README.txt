RECENT NOTES
1) BcpRoutingEngine should now be done, please take a look at it. Since the original code "implemented" the 
RouterForwarderIF interface, I had to just implement it inside BcpRoutingEngine like it was always a native 
part of it.

2) I implemented callbacks using function pointers, I know we will have to do this in other places, but the
way I implemented is not optimal (there are different "Register" functions for each event, instead of just one
that can figure it out for itself).

3) DelayQueue is NOT used anywhere in BCP. Do not spend the time reimplementing (as far as I can tell . . . ?).

4) message_t is up for debate. I don't think its 'really' needed. It appears to serve as a temporary buffer. 
	I think we can get the same functionality of message_t from using the packetbuf in Contiki... investigating.

GENERAL COMMANDS
make motelist			-	list connected motes
make <program>.upload	-	upload program to all motes
make serialview			-	connect to mote and see serial data

COMMAND OPTIONS
OTES=/dev/ttyUSB<#>	-	set specific node

SPECIFIC COMMANDS
make burn-nodeid.upload	MOTES=/dev/ttyUSB<#> nodeid=<#> nodemac=<#>	-	burn the node into the specified node

CHECK FOR BROADCAST
is_broadcast = rimeaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),
                                &rimeaddr_null);
	
