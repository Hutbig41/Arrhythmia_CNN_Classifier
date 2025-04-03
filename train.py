import numpy as np
import pandas as pd
import tensorflow as tf
from tensorflow.keras.models import Sequential
from tensorflow.keras.layers import Dense, Conv1D, MaxPooling1D, Flatten
from tensorflow.keras.utils import to_categorical
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import StandardScaler, LabelEncoder
import wfdb  # WaveForm DataBase package to handle MIT-BIH dataset
import os
import joblib

# Example function to load and preprocess the MIT-BIH dataset
def load_mit_bih_data():
    # MIT-BIH dataset path (replace with actual path)
    data_path = 'MIT_DATA'
    record_names = ['100', '101', '102', '103', '104', '105', '106', '107', '108', '109', '111', '112', '113', '114', '115', '116', '117', '118', '119', '121', '122', '123', '124', '200', '201', '202', '203', '205', '207', '208', '209', '210', '212', '213', '214', '215', '217', '219', '220', '221', '222', '223', '228', '230', '231', '232', '233', '234']  # Example records, add more as needed

    X = []
    y = []

    for record_name in record_names:
        record_path = os.path.join(data_path, record_name)
        try:
            record = wfdb.rdrecord(record_path)
            annotation = wfdb.rdann(record_path, 'atr')
        except FileNotFoundError:
            print(f"File not found: {record_path}")
            continue
        except OverflowError as e:
            print(f"Error reading annotation file for {record_path}: {e}")
            continue

        # Extract ECG signal and corresponding annotations
        signal = record.p_signal[:, 0]  # Using the first channel (lead)
        annotations = annotation.sample

        for i in range(len(annotations) - 1):
            start = annotations[i]
            end = annotations[i + 1]

            if start + 300 < len(signal):  # Ensuring enough data points (e.g., 300 samples)
                X.append(signal[start:start + 300])
                y.append(annotation.symbol[i])

    X = np.array(X)
    y = np.array(y)

    if len(X) == 0 or len(y) == 0:
        raise ValueError("No data was loaded. Please check the data files and paths.")

    # Print all unique labels to ensure they are all mapped
    unique_labels = set(y)
    print(f"Unique labels in the dataset: {unique_labels}")

    # Convert labels to numerical values using LabelEncoder
    label_encoder = LabelEncoder()
    y_encoded = label_encoder.fit_transform(y)

    # Save the LabelEncoder to a file
    joblib.dump(label_encoder, "label_encoder.pkl")

    return X, y_encoded

# Load and preprocess the data
try:
    X, y = load_mit_bih_data()
except ValueError as e:
    print(f"Error: {e}")
    exit()

# Convert labels to categorical one-hot encoding
y = to_categorical(y)

# Split the data into training and testing sets
X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)

# Standardize the features
scaler = StandardScaler()
X_train = scaler.fit_transform(X_train.reshape(-1, X_train.shape[-1])).reshape(X_train.shape)
X_test = scaler.transform(X_test.reshape(-1, X_test.shape[-1])).reshape(X_test.shape)

# Reshape the data for Conv1D (need 3D input: samples, timesteps, features)
X_train = X_train.reshape((X_train.shape[0], X_train.shape[1], 1))
X_test = X_test.reshape((X_test.shape[0], X_test.shape[1], 1))

# Build the model
model = Sequential([
    Conv1D(filters=32, kernel_size=3, activation='relu', input_shape=(X_train.shape[1], 1)),
    MaxPooling1D(pool_size=2),
    Flatten(),
    Dense(100, activation='relu'),
    Dense(y.shape[1], activation='softmax')
])

# Compile the model
model.compile(optimizer='adam', loss='categorical_crossentropy', metrics=['accuracy'])

# Train the model
model.fit(X_train, y_train, epochs=50, batch_size=32, validation_data=(X_test, y_test))

# Evaluate the model
loss, accuracy = model.evaluate(X_test, y_test)
print(f'Accuracy: {accuracy}')
model.save("hutcmod.h5")