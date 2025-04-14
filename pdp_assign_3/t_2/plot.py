import pandas as pd
import matplotlib.pyplot as plt

data = pd.read_csv('timings.csv')

plt.bar(data['method'], data['time_seconds'], color=['skyblue', 'lightgreen', 'salmon'])
plt.ylabel("Execution Time (seconds)")
plt.title("Matrix Multiplication Benchmark: Serial vs OpenMP vs Distributed")
plt.grid(axis='y', linestyle='--', alpha=0.7)
plt.tight_layout()
plt.show()
