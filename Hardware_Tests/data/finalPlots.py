import matplotlib.pyplot as plt
import pandas as pd
import numpy as np
import sys

df= pd.read_csv(sys.argv[1])
RX= int(sys.argv[2]);
RX_floor= 2336.61
if(RX):
    print("RX: dropping rows below power floor\n");
    df = df[df['P (mW)'] > RX_floor]


plotting_data= df.to_numpy()
t= plotting_data[:,0]
P= plotting_data[:,1]


fit = np.polyfit(t, P, 2)
a = fit[0]
b = fit[1]
c = fit[2]
quadratic= a * np.square(t) + b * t + c

fit = np.polyfit(t, P, 1)
m = fit[0]
b = fit[1]
linear= m * t + b

fig1= plt.figure()
ax1= fig1.subplots()

graph_title= ''
i=0
for arg in sys.argv:
    if(arg != '' and i>2):
        graph_title= graph_title + ' ' + str(arg)
    i+=1

avg_calc= 'avg power: ' + str(round(P.mean(), 3))  + ' mW'
ax1.set_title(graph_title)
ax1.plot(t, quadratic, color = 'red', alpha = 1, label = 'Quadratic fit')
ax1.plot(t, linear, color = 'orange', alpha = 1, label = 'Linear fit')
ax1.scatter(t, P, s = 5, color = 'blue', label = avg_calc)
ax1.legend()
ax1.set_xlabel('time (ms)')
ax1.set_ylabel('power (mW)')
plt.show()
