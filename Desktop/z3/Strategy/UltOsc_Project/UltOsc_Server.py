# =================================================================
# ULTOSC ML SERVER — Param Prediction + GO/SKIP Filter
#
# Model 1: PARAM model — 6 features -> 5 UltOsc params
#   CSV: UltOsc_TrainingData.csv
#   Endpoint: POST /predict
#
# Model 2: FILTER model — 6 features + direction -> WIN/LOSS
#   CSV: UltOsc_TradeData.csv
#   Endpoint: POST /filter
#
# Usage:
#   python UltOsc_Server.py train
#   python UltOsc_Server.py serve
#   python UltOsc_Server.py         -> train + serve
#
# Port: 5002 (nem ütközik MLDRIVEN 5001-gyel)
# =================================================================

import sys
import numpy as np
import pandas as pd
import pickle
from pathlib import Path

MODEL_DIR = Path(__file__).parent / "ultosc_models"
MODEL_DIR.mkdir(exist_ok=True)

BASE_DIR = Path(__file__).parent.parent.parent  # z3/ (from UltOsc_Project -> Strategy -> z3)

CSV_PARAMS = BASE_DIR / "UltOsc_TrainingData.csv"
CSV_TRADES = BASE_DIR / "UltOsc_TradeData.csv"

# 6 input features
FEATURE_COLS = [
    'ATR_Pct', 'Range_Pct', 'Volatility', 'RSI', 'Hurst', 'Current_State'
]

# 5 target params
TARGET_COLS = [
    'Edge', 'Threshold_x10', 'Stop_x10', 'Cooldown', 'MaxLayers'
]

PARAM_CONFIG = {
    'Edge':           {'min': 10,  'max': 30,  'type': 'int'},
    'Threshold_x10':  {'min': 2,   'max': 15,  'type': 'int'},
    'Stop_x10':       {'min': 15,  'max': 50,  'type': 'int'},
    'Cooldown':       {'min': 6,   'max': 24,  'type': 'int'},
    'MaxLayers':      {'min': 1,   'max': 6,   'type': 'int'},
}


def train_param_model():
    from xgboost import XGBRegressor
    from sklearn.preprocessing import StandardScaler
    from sklearn.model_selection import train_test_split
    from sklearn.multioutput import MultiOutputRegressor

    print(f"\n=== ULTOSC PARAM MODEL TRAINING ===")
    print(f"CSV: {CSV_PARAMS}")

    if not CSV_PARAMS.exists():
        print(f"ERROR: {CSV_PARAMS} not found!")
        return False

    df = pd.read_csv(CSV_PARAMS)
    print(f"Rows: {len(df)}")

    missing = [c for c in FEATURE_COLS + TARGET_COLS if c not in df.columns]
    if missing:
        print(f"Missing columns: {missing}")
        return False

    for col in FEATURE_COLS + TARGET_COLS:
        df[col] = pd.to_numeric(df[col], errors='coerce')
    df = df.dropna(subset=FEATURE_COLS + TARGET_COLS)
    print(f"Valid rows: {len(df)}")

    if len(df) < 20:
        print("WARNING: Very few samples!")

    X = df[FEATURE_COLS].values.astype(float)
    y = df[TARGET_COLS].values.astype(float)

    scaler = StandardScaler()
    X_s = scaler.fit_transform(X)
    X_train, X_test, y_train, y_test = train_test_split(X_s, y, test_size=0.2, random_state=42)

    base = XGBRegressor(n_estimators=200, max_depth=4, learning_rate=0.05,
                        subsample=0.8, colsample_bytree=0.8, random_state=42)
    model = MultiOutputRegressor(base)
    model.fit(X_train, y_train)

    from sklearn.metrics import mean_absolute_error, r2_score
    y_pred = model.predict(X_test)
    print(f"MAE: {mean_absolute_error(y_test, y_pred):.4f}  R2: {r2_score(y_test, y_pred):.4f}")

    for i, col in enumerate(TARGET_COLS):
        mae = mean_absolute_error(y_test[:, i], y_pred[:, i])
        cfg = PARAM_CONFIG[col]
        print(f"  {col:15s}: MAE={mae:.2f}  [{cfg['min']}-{cfg['max']}]")

    with open(MODEL_DIR / "param_model.pkl", 'wb') as f:
        pickle.dump(model, f)
    with open(MODEL_DIR / "param_scaler.pkl", 'wb') as f:
        pickle.dump(scaler, f)

    print("Param model saved.")
    return True


def train_filter_model():
    from xgboost import XGBClassifier
    from sklearn.preprocessing import StandardScaler
    from sklearn.model_selection import train_test_split

    print(f"\n=== ULTOSC FILTER MODEL TRAINING ===")
    print(f"CSV: {CSV_TRADES}")

    if not CSV_TRADES.exists():
        print(f"WARNING: {CSV_TRADES} not found — filter model skipped")
        return False

    df = pd.read_csv(CSV_TRADES)
    print(f"Rows: {len(df)}")

    if len(df) < 50:
        print("WARNING: Too few trades for reliable filter model")

    feat_cols = FEATURE_COLS
    missing = [c for c in feat_cols + ['Result'] if c not in df.columns]
    if missing:
        print(f"Missing columns: {missing}")
        return False

    for col in feat_cols:
        df[col] = pd.to_numeric(df[col], errors='coerce')
    df['Result'] = pd.to_numeric(df['Result'], errors='coerce')
    df = df.dropna(subset=feat_cols + ['Result'])

    df['Dir_num'] = df['Direction'].apply(lambda x: 1 if x == 1 else 0)

    X = df[feat_cols + ['Dir_num']].values.astype(float)
    y = df['Result'].values.astype(int)

    scaler = StandardScaler()
    X_s = scaler.fit_transform(X)
    X_train, X_test, y_train, y_test = train_test_split(X_s, y, test_size=0.2, random_state=42)

    model = XGBClassifier(
        n_estimators=150, max_depth=3, learning_rate=0.05,
        subsample=0.8, colsample_bytree=0.8,
        scale_pos_weight=max(1, (1 - y.mean()) / max(y.mean(), 0.01)),
        random_state=42, eval_metric='logloss'
    )
    model.fit(X_train, y_train)

    from sklearn.metrics import accuracy_score
    y_pred = model.predict(X_test)
    acc = accuracy_score(y_test, y_pred)
    print(f"  Accuracy={acc:.2%}, Trades={len(df)}, WinRate={y.mean():.2%}")

    with open(MODEL_DIR / "filter_model.pkl", 'wb') as f:
        pickle.dump(model, f)
    with open(MODEL_DIR / "filter_scaler.pkl", 'wb') as f:
        pickle.dump(scaler, f)

    print("Filter model saved.")
    return True


def load_models():
    result = {'param_model': None, 'param_scaler': None,
              'filter_model': None, 'filter_scaler': None}

    p1 = MODEL_DIR / "param_model.pkl"
    p2 = MODEL_DIR / "param_scaler.pkl"
    if p1.exists() and p2.exists():
        with open(p1, 'rb') as f: result['param_model'] = pickle.load(f)
        with open(p2, 'rb') as f: result['param_scaler'] = pickle.load(f)
        print("Param model loaded.")

    f1 = MODEL_DIR / "filter_model.pkl"
    f2 = MODEL_DIR / "filter_scaler.pkl"
    if f1.exists() and f2.exists():
        with open(f1, 'rb') as f: result['filter_model'] = pickle.load(f)
        with open(f2, 'rb') as f: result['filter_scaler'] = pickle.load(f)
        print("Filter model loaded.")

    return result


def serve(port=5002):
    from flask import Flask, request, jsonify

    state = load_models()
    if state['param_model'] is None:
        print("Training models first...")
        train_param_model()
        train_filter_model()
        state = load_models()

    app = Flask(__name__)

    @app.route('/predict', methods=['POST'])
    def api_predict():
        try:
            data = request.get_json(force=True)
            features = data.get('features', [])
            if len(features) != len(FEATURE_COLS):
                return jsonify({"error": f"Expected {len(FEATURE_COLS)} features"}), 400

            X = np.array(features).reshape(1, -1)
            X_s = state['param_scaler'].transform(X)
            pred = state['param_model'].predict(X_s)[0]

            result = {}
            for i, col in enumerate(TARGET_COLS):
                cfg = PARAM_CONFIG[col]
                val = int(round(float(np.clip(pred[i], cfg['min'], cfg['max']))))
                result[col] = val
            return jsonify(result)
        except Exception as e:
            return jsonify({"error": str(e)}), 500

    @app.route('/filter', methods=['POST'])
    def api_filter():
        try:
            data = request.get_json(force=True)
            features = data.get('features', [])
            direction = data.get('direction', 1)

            if state['filter_model'] is None:
                return jsonify({"action": "GO", "confidence": 0.5, "model": "none"})

            dir_num = 1 if direction == 1 else 0
            X = np.array(features + [dir_num]).reshape(1, -1)
            X_s = state['filter_scaler'].transform(X)

            prob = state['filter_model'].predict_proba(X_s)[0]
            win_prob = float(prob[1]) if len(prob) > 1 else 0.5

            threshold = 0.35
            action = "GO" if win_prob >= threshold else "SKIP"

            return jsonify({
                "action": action,
                "confidence": round(win_prob, 4)
            })
        except Exception as e:
            return jsonify({"action": "GO", "confidence": 0.5, "error": str(e)})

    @app.route('/retrain', methods=['POST'])
    def api_retrain():
        try:
            ok1 = train_param_model()
            ok2 = train_filter_model()
            state.update(load_models())
            return jsonify({"param": ok1, "filter": ok2})
        except Exception as e:
            return jsonify({"error": str(e)}), 500

    @app.route('/health', methods=['GET'])
    def health():
        return jsonify({
            "status": "ok",
            "version": "UltOsc v1",
            "param_model": state['param_model'] is not None,
            "param_targets": TARGET_COLS,
            "filter_model": state['filter_model'] is not None
        })

    print(f"\n=== ULTOSC ML SERVER on port {port} ===")
    print(f"Params: {len(TARGET_COLS)} ({', '.join(TARGET_COLS)})")
    print(f"POST /predict  -> {len(TARGET_COLS)} optimal params")
    print(f"POST /filter   -> GO/SKIP + confidence")
    print(f"POST /retrain  -> retrain both models")
    print(f"GET  /health   -> status\n")
    app.run(host='127.0.0.1', port=port, debug=False)


if __name__ == '__main__':
    mode = sys.argv[1] if len(sys.argv) > 1 else "all"
    if mode == "train":
        train_param_model()
        train_filter_model()
    elif mode == "serve":
        serve()
    else:
        train_param_model()
        train_filter_model()
        serve()
