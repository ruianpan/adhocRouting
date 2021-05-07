'''
Gets power consumption data from the MCU over serial UART,
the format of the incoming power-consumption data is: (timestamp_ms, power_mW)
Incoming data is parsed into csv for later analysis

Date: May 3rd, 2021
Author: Arden Diakhate-Palme
'''
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import serial 
import csv

ser= serial.Serial('/dev/cu.usbmodem141301', 115200, timeout=10) 
total_data= list() 
while True:
        line= ser.readline()   # read a '\n' terminated line
        line= line.decode('UTF-8');
        line= line[:-2]

        #parse a line of serial data, filling columns
        done=0
        prev_item= 1
        if(len(line) < 27 and len(line) > 0):
            tmp_lst= list()
            for data in line.split(','):
                item= int(data)
                tmp_lst.append(data)
                if(item == 0 and prev_item == 0):
                    done=1
                    break
                prev_item= item

            total_data.append(tmp_lst);

        if(done == 1):
            break

print("Data collection complete")
total_data= total_data[:-1] #remove last "done" signal from data

baseline= 2285 
df= pd.DataFrame(total_data, columns = ["t (ms)", "P (mW)"])
for i in df.index:
    df.iloc[i, 1]= str( int(df.iloc[i, 1]) - baseline);
    
#write aggregated current-consumption data into the destination file
df.to_csv('olsr_rx.csv', ',', index=False);

