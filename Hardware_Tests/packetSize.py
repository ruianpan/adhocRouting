'''
@brief Gathers average power (mW) based on sent packets UDP (TX side)
@date  4/26/21
'''

import time
import board
import adafruit_ina260
import socket

#current sensor inits
i2c = board.I2C()
ina260 = adafruit_ina260.INA260(i2c)

#UDP inits
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM) 


num_samples= 400;   #number of samples to average over
T_ms= 100;          #sampling period
samples= list()
while(i<num_samples):
    samples.append(ina260.power) 
    send_packet();
    time.sleep(T_ms/1000);
    i++;


