import neurokit2 as nk
import matplotlib.pyplot as plt

# Generate synthetic ECG signal
ecg_signal = nk.ecg_simulate(duration=10, sampling_rate=1000)

# Plot the ECG
plt.plot(ecg_signal)
plt.title("Simulated ECG Signal")
plt.xlabel("Time (ms)")
plt.ylabel("Amplitude")
plt.show()
