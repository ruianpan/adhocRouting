import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import serial 
import csv

ser= serial.Serial('/dev/cu.usbmodem141301', 115200, timeout=5) 
total_data= list() 
while True:
        line= ser.readline()   # read a '\n' terminated line
        line= line.decode('UTF-8');
        line= line[:-2]

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

total_data= total_data[:-1] #remove last "done" signal from data
print(total_data)

df= pd.DataFrame(total_data, columns = ["t (ms)", "P (mW)"])
df.to_csv('dsdv_tx_power.csv', ',', index=False);

