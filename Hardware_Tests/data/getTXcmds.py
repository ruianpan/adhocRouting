import pandas as pd
df= pd.read_csv('dsdv.csv')

#columns of new csv

size_hdr= 14 #ethernet frame hdr size
s_to_us= 1000000.0 #us in a sec
time_curr=0;
time_prev=0;
total_data= list();
for i in df.index:
    data_row= list()

    #is it a broadcast frame?
    if( (df['Destination Address'][i] == "10.1.1.255") and (df['Protocols in frame'][i] == "wlan:llc:ip:udp:olsr") ):
        data_row.append(1);
    else:
        data_row.append(0);

    #time delta 
    time_curr= df['Time'][i]
    tmp= float(time_curr) - float(time_prev)
    delta= int(tmp * s_to_us);
    data_row.append(delta);
    time_prev= time_curr

    #bytes to transmit
    num_bytes= df['Frame length stored into the capture file'][i];
    data_row.append(num_bytes - size_hdr)
    total_data.append(data_row)

out_df = pd.DataFrame(total_data, columns = ["broadcast", "wait_time", "bytes_to_tx"])

out_df.to_csv('dsdv_out.csv', ',', index=False);
