# =================================================================
# META-LEARNING SERVER v6 - Dual Algo (SMA + CH)
#
# Model 1: PARAM model — market features -> 12 algo-specific params
#   CSV: MLTrainingData.csv
#   Endpoint: POST /predict
#
# Model 2: FILTER model — trade features -> WIN/LOSS
#   CSV: MLTradeData.csv
#   Endpoint: POST /filter
#
# Usage:
#   python TENSORFLOWMODEL.py train
#   python TENSORFLOWMODEL.py serve
#   python TENSORFLOWMODEL.py         -> train + serve
# =================================================================

import sys
import numpy as np
import pandas as pd
import pickle
from pathlib import Path

MODEL_DIR = Path(__file__).parent / "meta_models"
MODEL_DIR.mkdir(exist_ok=True)

BASE_DIR = Path(__file__).parent.parent  # z3/

CSV_PARAMS = BASE_DIR / "MLTrainingData.csv"
CSV_TRADES = BASE_DIR / "MLTradeData.csv"

# 12 input features
FEATURE_COLS = [
    'ATR_Pct', 'Range_Pct', 'Volatility', 'ADX', 'Trend_Bias',
    'Trend_Quality', 'RSI', 'Hurst', 'Return_20', 'BB_Width',
    'WinRate', 'Current_State'
]

# 12 target params (6 SMA + 6 CH)
TARGET_COLS = [
    'smaTF', 'FastMA', 'SlowMA', 'smaStop_x10', 'adxSMA', 'mmiSMA',
    'N', 'Factor_x100', 'chStop_x10', 'lifeTime', 'adxCH', 'mmiCH'
]

PARAM_CONFIG = {
    'smaTF':        {'min': 1,   'max': 8,   'type': 'int'},
    'FastMA':       {'min': 10,  'max': 40,  'type': 'int'},
    'SlowMA':       {'min': 40,  'max': 100, 'type': 'int'},
    'smaStop_x10':  {'min': 15,  'max': 50,  'type': 'int'},
    'adxSMA':       {'min': 15,  'max': 40,  'type': 'int'},
    'mmiSMA':       {'min': 60,  'max': 85,  'type': 'int'},
    'N':            {'min': 30,  'max': 120, 'type': 'int'},
    'Factor_x100':  {'min': 10,  'max': 40,  'type': 'int'},
    'chStop_x10':   {'min': 15,  'max': 50,  'type': 'int'},
    'lifeTime':     {'min': 10,  'max': 40,  'type': 'int'},
    'adxCH':        {'min': 15,  'max': 40,  'type': 'int'},
    'mmiCH':        {'min': 60,  'max': 85,  'type': 'int'},
}


def train_param_model():
    from xgboost import XGBRegressor
    from sklearn.preprocessing import StandardScaler
    from sklearn.model_selection import train_test_split
    from sklearn.multioutput import MultiOutputRegressor

    print(f"\n=== PARAM MODEL TRAINING (v6: 12 params) ===")
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

    print(f"\n=== FILTER MODEL TRAINING ===")
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
    df['Type_num'] = df['EntryType'].apply(lambda x: 1 if x == 'CH' else 0)

    models = {}
    scalers = {}

    for entry_type, type_name in [(0, 'SMA'), (1, 'CH')]:
        mask = df['Type_num'] == entry_type
        if mask.sum() < 30:
            print(f"  {type_name}: only {mask.sum()} trades, skipping")
            continue

        X_sub = df.loc[mask, feat_cols + ['Dir_num']].values.astype(float)
        y_sub = df.loc[mask, 'Result'].values.astype(int)

        scaler = StandardScaler()
        X_s = scaler.fit_transform(X_sub)
        X_train, X_test, y_train, y_test = train_test_split(X_s, y_sub, test_size=0.2, random_state=42)

        model = XGBClassifier(
            n_estimators=150, max_depth=3, learning_rate=0.05,
            subsample=0.8, colsample_bytree=0.8,
            scale_pos_weight=max(1, (1 - y_sub.mean()) / max(y_sub.mean(), 0.01)),
            random_state=42, use_label_encoder=False, eval_metric='logloss'
        )
        model.fit(X_train, y_train)

        from sklearn.metrics import accuracy_score
        y_pred = model.predict(X_test)
        acc = accuracy_score(y_test, y_pred)
        print(f"  {type_name}: Accuracy={acc:.2%}, Trades={mask.sum()}, WinRate={y_sub.mean():.2%}")

        models[type_name] = model
        scalers[type_name] = scaler

    with open(MODEL_DIR / "filter_models.pkl", 'wb') as f:
        pickle.dump(models, f)
    with open(MODEL_DIR / "filter_scalers.pkl", 'wb') as f:
        pickle.dump(scalers, f)

    print("Filter models saved.")
    return True


def load_models():
    result = {'param_model': None, 'param_scaler': None,
              'filter_models': {}, 'filter_scalers': {}}

    p1 = MODEL_DIR / "param_model.pkl"
    p2 = MODEL_DIR / "param_scaler.pkl"
    if p1.exists() and p2.exists():
        with open(p1, 'rb') as f: result['param_model'] = pickle.load(f)
        with open(p2, 'rb') as f: result['param_scaler'] = pickle.load(f)
        print("Param model loaded.")

    f1 = MODEL_DIR / "filter_models.pkl"
    f2 = MODEL_DIR / "filter_scalers.pkl"
    if f1.exists() and f2.exists():
        with open(f1, 'rb') as f: result['filter_models'] = pickle.load(f)
        with open(f2, 'rb') as f: result['filter_scalers'] = pickle.load(f)
        print(f"Filter models loaded: {list(result['filter_models'].keys())}")

    return result


def serve(port=5001):
    from flask import Flask, request, jsonify

    state = load_models()
    if state['param_model'] is None:
        print("Training param model first...")
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
            entry_type = data.get('entry_type', 'CH')

            if entry_type not in state['filter_models']:
                return jsonify({"action": "GO", "confidence": 0.5, "model": "none"})

            model = state['filter_models'][entry_type]
            scaler = state['filter_scalers'][entry_type]

            dir_num = 1 if direction == 1 else 0
            X = np.array(features + [dir_num]).reshape(1, -1)
            X_s = scaler.transform(X)

            prob = model.predict_proba(X_s)[0]
            win_prob = float(prob[1]) if len(prob) > 1 else 0.5

            threshold = 0.40 if entry_type == 'CH' else 0.45
            action = "GO" if win_prob >= threshold else "SKIP"

            return jsonify({
                "action": action,
                "confidence": round(win_prob, 4),
                "model": entry_type
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
            "version": "v6",
            "param_model": state['param_model'] is not None,
            "param_targets": TARGET_COLS,
            "filter_models": list(state['filter_models'].keys())
        })

    print(f"\n=== META-LEARNING SERVER v6 on port {port} ===")
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
