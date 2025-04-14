import pandas as pd
import matplotlib.pyplot as plt

# Read the timings CSV
data = pd.read_csv('timings.csv')


plt.bar(
    data['method'],
    data['time'],             
    color=['skyblue', 'lightgreen', 'salmon']
)


plt.ylabel("Execution Time (seconds)")
plt.title("Summation Benchmark: Serial vs OpenMP vs Distributed")


plt.grid(axis='y', linestyle='--', alpha=0.7)


plt.tight_layout()


plt.show()
