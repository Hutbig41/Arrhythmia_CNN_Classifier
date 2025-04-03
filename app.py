from flask import Flask, request, render_template, jsonify
import wfdb
import numpy as np
from tensorflow.keras.models import load_model
import joblib
import os
import logging
from collections import Counter

app = Flask(__name__)

# Set up logging
logging.basicConfig(level=logging.INFO)

# Load the trained model and label encoder
model = load_model("hutcmod.h5")
label_encoder = joblib.load("label_encoder.pkl")

# Ensure the uploads directory exists
os.makedirs("uploads", exist_ok=True)

# Home page
@app.route("/")
def home():
    return render_template("index.html")

@app.route("/predict", methods=["POST"])
def predict():
    try:
        # Check if files are uploaded
        if "hea_file" not in request.files or "dat_file" not in request.files:
            return jsonify({"error": "Please upload both .hea and .dat files"})

        hea_file = request.files["hea_file"]
        dat_file = request.files["dat_file"]

        # Save the uploaded files temporarily
        hea_path = os.path.join("uploads", hea_file.filename)
        dat_path = os.path.join("uploads", dat_file.filename)
        hea_file.save(hea_path)
        dat_file.save(dat_path)

        # Load the ECG data using wfdb
        record_name = os.path.splitext(hea_file.filename)[0]
        try:
            signals, fields = wfdb.rdsamp(os.path.join("uploads", record_name))
        except Exception as e:
            logging.error(f"Error reading WFDB record: {e}")
            return jsonify({"error": f"Error reading WFDB record: {e}"})

        # Preprocess the ECG signal for the model
        ecg_signal = signals[:, 0]  # Use the first lead (e.g., MLII)

        # Reshape or segment the ECG signal to match the model's input shape
        sequence_length = model.input_shape[1]  # Get the expected sequence length
        if len(ecg_signal) < sequence_length:
            logging.error("ECG signal is too short.")
            return jsonify({"error": f"ECG signal is too short. Expected at least {sequence_length} time steps."})

        # Segment the ECG signal
        ecg_segments = []
        for i in range(0, len(ecg_signal) - sequence_length + 1, sequence_length):
            segment = ecg_signal[i:i + sequence_length]
            ecg_segments.append(segment)
        if len(ecg_signal) % sequence_length != 0:
            last_segment = ecg_signal[-sequence_length:]
            ecg_segments.append(last_segment)

        # Convert to numpy array and reshape for model input
        ecg_segments = np.array(ecg_segments).reshape(-1, sequence_length, 1)

        # Make predictions
        probabilities = model.predict(ecg_segments, verbose=0)  # Suppress TensorFlow logging
        predicted_class_indices = np.argmax(probabilities, axis=1)
        predictions = label_encoder.inverse_transform(predicted_class_indices)

        # Compute arrhythmia probabilities
        arrhythmia_counts = Counter(predictions)
        arrhythmia_probabilities = {
            arrhythmia: round((arrhythmia_counts[arrhythmia] / len(predictions)) * 100, 2)
            for arrhythmia in arrhythmia_counts
        }

        # Clean up uploaded files
        os.remove(hea_path)
        os.remove(dat_path)

        # Return all arrhythmia predictions and their probabilities
        return jsonify({"arrhythmias": arrhythmia_probabilities})

    except Exception as e:
        logging.error(f"Error during prediction: {e}")
        return jsonify({"error": str(e)})


if __name__ == "__main__":
    app.run(debug=True)