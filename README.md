# WebSocket-RX-TX-Test-Performance-Tool

BMC RX steps:
1.Mount SD card to BMC
2.BMC side command: ./websocketServer [PORT] RX
3.Client-side "WebSocket RX/TX Test Performance Tool" enter IP Address and Port
4.Browse test file
5.click "RX connect to BMC" button

BMC TX steps:
1.Put Test file to SD card
2.Mount SD card to BMC
3.BMC side command: ./websocketServer [PORT] TX
4.Client-side "WebSocket RX/TX Test Performance Tool" enter IP Address and Port
5.click "TX connect to BMC" button


Iperf steps:
BMC: ./iperf3 -s
Client RX: iperf3 -c 192.168.0.164
Client TX: iperf3 -c 192.168.0.164 -R

