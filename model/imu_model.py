import numpy as np
import pandas as pd
from sklearn.ensemble import RandomForestClassifier
from sklearn.model_selection import train_test_split

def make_windows(arr, window_size=50, step=25):
    windows = []
    for start in range(0, len(arr) - window_size + 1, step):
        windows.append(arr[start:start + window_size])
    return np.array(windows)

def imu_features(window):
    return {
        "imu_max": np.max(window),
        "imu_min": np.min(window),
        "imu_ptp": np.ptp(window),
        "imu_rms": np.sqrt(np.mean(window**2)),
        "imu_var": np.var(window)
    }

def build_dataset(df, label):
    imu = df['imu'].values
    windows = make_windows(imu, window_size=100)

    rows = []
    # Calculate features for each window, assign each feature vector a label in dataframe
    for w in windows:
        feats = imu_features(w)
        feats['label'] = label
        rows.append(feats)

    return pd.DataFrame(rows)


# Building the datasets
noise = pd.read_csv('collected-data/noise.csv')
knock = pd.read_csv('collected-data/knock.csv')

# binary classification: 0 = noise, 1 = knock
noise_ds = build_dataset(noise, label=0) 
knock_ds = build_dataset(knock, label=1)

dataset = pd.concat([noise_ds, knock_ds]).sample(frac=1)

print("Dataset shape:", dataset.shape)

# MODEL TRAINING
X = dataset.drop(columns=['label'])
y = dataset['label']

X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2)

clf = RandomForestClassifier(n_estimators=200)
clf.fit(X_train, y_train)

print(f"Accuracy: {clf.score(X_test, y_test):.3f}")