import wfdb
import scipy.io
import pandas as pd
import numpy as np
import os

# Define the record to be processed
record_name = "A00126"  # Example record
path = r"D:\Project\training2017"  # Use raw string or double backslashes

# Load the .mat file
mat_data = scipy.io.loadmat(os.path.join(path, f"{record_name}.mat"))

# Extract signal (assuming key 'val' is used)
signal = mat_data['val'][0]  # Adjust index based on the dataset structure

# Load the .hea file to retrieve sampling frequency
hea_file = os.path.join(path, f"{record_name}.hea")
with open(hea_file, "r") as f:
    header = f.readlines()
    fs = int(header[0].split()[2])  # Extracting sampling frequency

# Generate time vector
time = np.arange(len(signal)) / fs

# Create a DataFrame
df = pd.DataFrame({"Time (s)": time, "MLII": signal})

# Save to CSV
csv_filename = os.path.join(path, f"{record_name}_ecg.csv")
df.to_csv(csv_filename, index=False)

print(f"ECG data saved to {csv_filename}")
