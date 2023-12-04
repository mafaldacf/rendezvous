#!/usr/bin/env python3

from pprint import pprint as pp
import matplotlib.pyplot as plt
import seaborn as sns
import pandas as pd
import numpy as np

sns.set_theme(style='ticks')
plt.figure(figsize=(10, 18), dpi=450)
plt.rcParams['axes.labelsize'] = 'small'

data = {
    'Application Size': np.linspace(10, 100),
    'Performance (Antipode)': np.linspace(10, 100)**2 + 5,
}
intercept_point = data['Performance (Antipode)'][2*len(data['Performance (Antipode)']) // 3]
data['Performance (Rendezvous)'] = -(np.linspace(10, 100)**2 + 5) + 2 * intercept_point
df = pd.DataFrame(data)

# remove x and y ticks
plt.xticks([])
plt.yticks([])

plt.ylabel('y') 
plt.xlabel('x')

ax = sns.lineplot(data=df, x='Application Size', y='Performance (Antipode)', label='Antipode', linewidth = 3)
ax = sns.lineplot(data=df, x='Application Size', y='Performance (Rendezvous)', label='Rendezvous', linewidth = 3)

plt.xlabel('- Application Size +')
plt.ylabel('- Performance +')

# move the legend to the top right corner
plt.legend(loc='upper right', bbox_to_anchor=(1.0, 1.0), fontsize='small')
# set x axis limit
plt.xlim(0, 120)
plt.show()

#plt.savefig("antipode-vs-rendezvous")
#print(f"[INFO] Saved plot")