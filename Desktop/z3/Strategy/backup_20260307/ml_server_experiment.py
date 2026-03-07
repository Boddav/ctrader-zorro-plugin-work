# =================================================================
# ML SERVER EXPERIMENT - Model comparison for ML-DRIVEN
#
# Usage:
#   python ml_server_experiment.py train          -> train ALL models, show scores
#   python ml_server_experiment.py serve xgb      -> serve with XGBoost (default)
#   python ml_server_experiment.py serve rf       -> serve with RandomForest
#   python ml_server_experiment.py serve mlp      -> serve with Neural Network
#   python ml_server_experiment.py serve ridge    -> serve with Ridge/Logistic
#   python ml_server_experiment.py serve ensemble -> serve with Ensemble (all)
#
# Same endpoints as TENSORFLOWMODEL.py:
#   POST /predict  -> 12 params
#   POST /filter   -> GO/SKIP
#   GET  /health   -> status
# =================================================================

import sys
import numpy as np
import pandas as pd
import pickle
from pathlib import Path

MODEL_DIR = Path(__file__).parent / "meta_models"
MODEL_DIR.mkdir(exist_ok=True)

BASE_DIR = Path(__file__).parent.parent  # z3/

# Try backup CSVs first, then default location
BACKUP_DIR = Path(__file__).parent / "c backup"
CSV_PARAMS = BACKUP_DIR / "MLTrainingData.csv" if (BACKUP_DIR / "MLTrainingData.csv").exists() else BASE_DIR / "MLTrainingData.csv"
CSV_TRADES = BACKUP_DIR / "MLTradeData.csv" if (BACKUP_DIR / "MLTradeData.csv").exists() else BASE_DIR / "MLTradeData.csv"

FEATURE_COLS = [
    'ATR_Pct', 'Range_Pct', 'Volatility', 'ADX', 'Trend_Bias',
    'Trend_Quality', 'RSI', 'Hurst', 'Return_20', 'BB_Width',
    'WinRate', 'Current_State'
]

TARGET_COLS = [
    'smaTF', 'FastMA', 'SlowMA', 'smaStop_x10', 'adxSMA', 'mmiSMA',
    'N', 'Factor_x100', 'chStop_x10', 'lifeTime', 'adxCH', 'mmiCH'
]

PARAM_CONFIG = {
    'smaTF':        {'min': 1,   'max': 8},
    'FastMA':       {'min': 10,  'max': 40},
    'SlowMA':       {'min': 40,  'max': 100},
    'smaStop_x10':  {'min': 15,  'max': 50},
    'adxSMA':       {'min': 15,  'max': 40},
    'mmiSMA':       {'min': 60,  'max': 85},
    'N':            {'min': 30,  'max': 120},
    'Factor_x100':  {'min': 10,  'max': 40},
    'chStop_x10':   {'min': 15,  'max': 50},
    'lifeTime':     {'min': 10,  'max': 40},
    'adxCH':        {'min': 15,  'max': 40},
    'mmiCH':        {'min': 60,  'max': 85},
}

# Filter thresholds (same as original)
SMA_THRESHOLD = 0.45
CH_THRESHOLD = 0.40


def load_param_data():
    print(f"Param CSV: {CSV_PARAMS}")
    df = pd.read_csv(CSV_PARAMS)
    for col in FEATURE_COLS + TARGET_COLS:
        df[col] = pd.to_numeric(df[col], errors='coerce')
    df = df.dropna(subset=FEATURE_COLS + TARGET_COLS)
    X = df[FEATURE_COLS].values.astype(float)
    y = df[TARGET_COLS].values.astype(float)
    print(f"  Rows: {len(df)}")
    return X, y


def load_filter_data():
    print(f"Filter CSV: {CSV_TRADES}")
    df = pd.read_csv(CSV_TRADES)
    feat_cols = FEATURE_COLS
    for col in feat_cols:
        df[col] = pd.to_numeric(df[col], errors='coerce')
    df['Result'] = pd.to_numeric(df['Result'], errors='coerce')
    df = df.dropna(subset=feat_cols + ['Result'])
    df['Dir_num'] = df['Direction'].apply(lambda x: 1 if x == 1 else 0)
    df['Type_num'] = df['EntryType'].apply(lambda x: 1 if x == 'CH' else 0)
    print(f"  Rows: {len(df)}")
    return df, feat_cols


def create_param_models():
    from sklearn.ensemble import RandomForestRegressor, GradientBoostingRegressor
    from sklearn.neural_network import MLPRegressor
    from sklearn.linear_model import Ridge
    from sklearn.multioutput import MultiOutputRegressor
    from xgboost import XGBRegressor
    from lightgbm import LGBMRegressor
    from catboost import CatBoostRegressor

    models = {
        'xgb': MultiOutputRegressor(XGBRegressor(
            n_estimators=200, max_depth=4, learning_rate=0.05,
            subsample=0.8, colsample_bytree=0.8, random_state=42)),
        'lgbm': MultiOutputRegressor(LGBMRegressor(
            n_estimators=200, max_depth=6, learning_rate=0.05,
            subsample=0.8, colsample_bytree=0.8, random_state=42, verbose=-1)),
        'catboost': MultiOutputRegressor(CatBoostRegressor(
            iterations=200, depth=4, learning_rate=0.05,
            random_seed=42, verbose=0)),
        'rf': MultiOutputRegressor(RandomForestRegressor(
            n_estimators=200, max_depth=8, min_samples_leaf=5, random_state=42)),
        'mlp': MLPRegressor(
            hidden_layer_sizes=(128, 64, 32), max_iter=500,
            learning_rate='adaptive', early_stopping=True,
            validation_fraction=0.15, random_state=42),
        'ridge': MultiOutputRegressor(Ridge(alpha=1.0)),
        'gbr': MultiOutputRegressor(GradientBoostingRegressor(
            n_estimators=150, max_depth=4, learning_rate=0.05,
            subsample=0.8, random_state=42)),
    }
    return models


def create_filter_models():
    from sklearn.ensemble import RandomForestClassifier, GradientBoostingClassifier
    from sklearn.neural_network import MLPClassifier
    from sklearn.linear_model import LogisticRegression
    from xgboost import XGBClassifier
    from lightgbm import LGBMClassifier
    from catboost import CatBoostClassifier

    def make_models(scale_pos_weight=1.0):
        return {
            'xgb': XGBClassifier(
                n_estimators=150, max_depth=3, learning_rate=0.05,
                subsample=0.8, colsample_bytree=0.8,
                scale_pos_weight=scale_pos_weight,
                random_state=42, eval_metric='logloss'),
            'lgbm': LGBMClassifier(
                n_estimators=150, max_depth=4, learning_rate=0.05,
                subsample=0.8, colsample_bytree=0.8,
                scale_pos_weight=scale_pos_weight,
                random_state=42, verbose=-1),
            'catboost': CatBoostClassifier(
                iterations=150, depth=3, learning_rate=0.05,
                scale_pos_weight=scale_pos_weight,
                random_seed=42, verbose=0),
            'rf': RandomForestClassifier(
                n_estimators=200, max_depth=6, min_samples_leaf=5,
                class_weight='balanced', random_state=42),
            'mlp': MLPClassifier(
                hidden_layer_sizes=(64, 32), max_iter=500,
                learning_rate='adaptive', early_stopping=True,
                validation_fraction=0.15, random_state=42),
            'ridge': LogisticRegression(
                C=1.0, class_weight='balanced', max_iter=1000, random_state=42),
            'gbr': GradientBoostingClassifier(
                n_estimators=150, max_depth=3, learning_rate=0.05,
                subsample=0.8, random_state=42),
        }
    return make_models


def train_all():
    from sklearn.preprocessing import StandardScaler
    from sklearn.model_selection import train_test_split
    from sklearn.metrics import mean_absolute_error, r2_score, accuracy_score

    print("\n" + "="*60)
    print("  MODEL COMPARISON - PARAM PREDICTION")
    print("="*60)

    X, y = load_param_data()
    scaler = StandardScaler()
    X_s = scaler.fit_transform(X)
    X_train, X_test, y_train, y_test = train_test_split(X_s, y, test_size=0.2, random_state=42)

    param_models = create_param_models()
    param_results = {}

    for name, model in param_models.items():
        try:
            model.fit(X_train, y_train)
            y_pred = model.predict(X_test)
            mae = mean_absolute_error(y_test, y_pred)
            r2 = r2_score(y_test, y_pred)
            param_results[name] = {'mae': mae, 'r2': r2, 'model': model}
            print(f"\n  [{name:8s}]  MAE={mae:.3f}  R2={r2:.4f}")
            for i, col in enumerate(TARGET_COLS):
                col_mae = mean_absolute_error(y_test[:, i], y_pred[:, i])
                cfg = PARAM_CONFIG[col]
                rng = cfg['max'] - cfg['min']
                pct = col_mae / rng * 100 if rng > 0 else 0
                print(f"    {col:15s}: MAE={col_mae:5.2f} ({pct:4.1f}% of range)")
        except Exception as e:
            print(f"\n  [{name:8s}]  ERROR: {e}")

    # Save all param models + scaler
    with open(MODEL_DIR / "experiment_param_models.pkl", 'wb') as f:
        pickle.dump({k: v['model'] for k, v in param_results.items()}, f)
    with open(MODEL_DIR / "experiment_param_scaler.pkl", 'wb') as f:
        pickle.dump(scaler, f)

    print("\n" + "="*60)
    print("  MODEL COMPARISON - FILTER (GO/SKIP)")
    print("="*60)

    df, feat_cols = load_filter_data()
    make_filter = create_filter_models()

    filter_results = {}

    for entry_type, type_name in [(0, 'SMA'), (1, 'CH')]:
        mask = df['Type_num'] == entry_type
        if mask.sum() < 30:
            print(f"\n  {type_name}: only {mask.sum()} trades, skipping")
            continue

        X_sub = df.loc[mask, feat_cols + ['Dir_num']].values.astype(float)
        y_sub = df.loc[mask, 'Result'].values.astype(int)
        win_rate = y_sub.mean()
        spw = max(1, (1 - win_rate) / max(win_rate, 0.01))

        f_scaler = StandardScaler()
        X_s = f_scaler.fit_transform(X_sub)
        X_train, X_test, y_train, y_test = train_test_split(X_s, y_sub, test_size=0.2, random_state=42)

        print(f"\n  --- {type_name} (trades={mask.sum()}, winrate={win_rate:.1%}) ---")

        f_models = make_filter(scale_pos_weight=spw)
        type_results = {}

        for name, model in f_models.items():
            try:
                model.fit(X_train, y_train)
                y_pred = model.predict(X_test)
                acc = accuracy_score(y_test, y_pred)

                if hasattr(model, 'predict_proba'):
                    proba = model.predict_proba(X_test)
                    if proba.shape[1] > 1:
                        win_probs = proba[:, 1]
                        threshold = SMA_THRESHOLD if type_name == 'SMA' else CH_THRESHOLD
                        go_rate = (win_probs >= threshold).mean()
                    else:
                        go_rate = 0.5
                else:
                    go_rate = y_pred.mean()

                type_results[name] = {'acc': acc, 'go_rate': go_rate, 'model': model, 'scaler': f_scaler}
                print(f"    [{name:8s}]  Acc={acc:.1%}  GO_rate={go_rate:.1%}")
            except Exception as e:
                print(f"    [{name:8s}]  ERROR: {e}")

        filter_results[type_name] = type_results

    # Save all filter models
    with open(MODEL_DIR / "experiment_filter_all.pkl", 'wb') as f:
        pickle.dump(filter_results, f)

    # Summary table
    print("\n" + "="*60)
    print("  SUMMARY")
    print("="*60)
    print(f"\n  PARAM models (lower MAE = better):")
    sorted_p = sorted(param_results.items(), key=lambda x: x[1]['mae'])
    for i, (name, res) in enumerate(sorted_p):
        marker = " <-- BEST" if i == 0 else ""
        print(f"    {i+1}. {name:8s}  MAE={res['mae']:.3f}  R2={res['r2']:.4f}{marker}")

    print(f"\n  FILTER models (higher Acc = better):")
    for type_name, models in filter_results.items():
        sorted_f = sorted(models.items(), key=lambda x: x[1]['acc'], reverse=True)
        for i, (name, res) in enumerate(sorted_f):
            marker = " <-- BEST" if i == 0 else ""
            print(f"    {type_name} {name:8s}  Acc={res['acc']:.1%}  GO={res['go_rate']:.1%}{marker}")

    print("\n  Models saved to meta_models/experiment_*.pkl")
    print("  Use: python ml_server_experiment.py serve <model_name>")


def serve(model_type='xgb', port=5001):
    from flask import Flask, request, jsonify
    from sklearn.preprocessing import StandardScaler

    print(f"\nLoading model type: {model_type}")

    # Load param models
    with open(MODEL_DIR / "experiment_param_models.pkl", 'rb') as f:
        all_param_models = pickle.load(f)
    with open(MODEL_DIR / "experiment_param_scaler.pkl", 'rb') as f:
        param_scaler = pickle.load(f)

    # Load filter models
    with open(MODEL_DIR / "experiment_filter_all.pkl", 'rb') as f:
        all_filter_results = pickle.load(f)

    # hybrid = LightGBM param + XGBoost filter
    filter_model_type = model_type
    if model_type == 'hybrid':
        param_model_list = [all_param_models['lgbm']]
        param_model_names = ['lgbm']
        filter_model_type = 'xgb'
        print(f"  HYBRID: param=lgbm, filter=xgb")
    elif model_type == 'ensemble':
        param_model_list = list(all_param_models.values())
        param_model_names = list(all_param_models.keys())
        filter_model_type = 'xgb'
        print(f"  Ensemble param: {param_model_names}")
    else:
        if model_type not in all_param_models:
            print(f"  ERROR: '{model_type}' not found. Available: {list(all_param_models.keys())}")
            return
        param_model_list = [all_param_models[model_type]]
        param_model_names = [model_type]
        filter_model_type = model_type
        print(f"  Param model: {model_type}")

    app = Flask(__name__)

    @app.route('/predict', methods=['POST'])
    def api_predict():
        try:
            data = request.get_json(force=True)
            features = data.get('features', [])
            if len(features) != len(FEATURE_COLS):
                return jsonify({"error": f"Expected {len(FEATURE_COLS)} features"}), 400

            X = np.array(features).reshape(1, -1)
            X_s = param_scaler.transform(X)

            if model_type == 'ensemble':
                preds = [m.predict(X_s)[0] for m in param_model_list]
                pred = np.median(preds, axis=0)
            else:
                pred = param_model_list[0].predict(X_s)[0]

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

            if entry_type not in all_filter_results:
                return jsonify({"action": "GO", "confidence": 0.5, "model": "none"})

            type_models = all_filter_results[entry_type]
            mt = filter_model_type
            if mt not in type_models:
                mt = list(type_models.keys())[0]

            model_info = type_models[mt]
            model = model_info['model']
            scaler = model_info['scaler']

            dir_num = 1 if direction == 1 else 0
            X = np.array(features + [dir_num]).reshape(1, -1)
            X_s = scaler.transform(X)

            if model_type == 'ensemble' and len(type_models) > 1:
                votes = []
                for m_name, m_info in type_models.items():
                    m = m_info['model']
                    s = m_info['scaler']
                    X_ms = s.transform(X)
                    if hasattr(m, 'predict_proba'):
                        p = m.predict_proba(X_ms)[0]
                        votes.append(float(p[1]) if len(p) > 1 else 0.5)
                    else:
                        votes.append(float(m.predict(X_ms)[0]))
                win_prob = np.mean(votes)
            elif hasattr(model, 'predict_proba'):
                prob = model.predict_proba(X_s)[0]
                win_prob = float(prob[1]) if len(prob) > 1 else 0.5
            else:
                win_prob = float(model.predict(X_s)[0])

            threshold = SMA_THRESHOLD if entry_type == 'SMA' else CH_THRESHOLD
            action = "GO" if win_prob >= threshold else "SKIP"

            return jsonify({
                "action": action,
                "confidence": round(win_prob, 4),
                "model": f"{entry_type}:{mt}"
            })
        except Exception as e:
            return jsonify({"action": "GO", "confidence": 0.5, "error": str(e)})

    @app.route('/algo_vote', methods=['POST'])
    def api_algo_vote():
        try:
            data = request.get_json(force=True)
            features = data.get('features', [])
            if len(features) != len(FEATURE_COLS):
                return jsonify({"error": f"Expected {len(FEATURE_COLS)} features"}), 400

            sma_go = False
            ch_go = False

            # Check SMA filter (direction=1 for long, test both)
            for direction in [1, -1]:
                dir_num = 1 if direction == 1 else 0

                if 'SMA' in all_filter_results:
                    type_models = all_filter_results['SMA']
                    mt = filter_model_type
                    if mt not in type_models:
                        mt = list(type_models.keys())[0]
                    m_info = type_models[mt]
                    X = np.array(features + [dir_num]).reshape(1, -1)
                    X_s = m_info['scaler'].transform(X)
                    if hasattr(m_info['model'], 'predict_proba'):
                        prob = m_info['model'].predict_proba(X_s)[0]
                        wp = float(prob[1]) if len(prob) > 1 else 0.5
                    else:
                        wp = float(m_info['model'].predict(X_s)[0])
                    if wp >= SMA_THRESHOLD:
                        sma_go = True

                if 'CH' in all_filter_results:
                    type_models = all_filter_results['CH']
                    mt = filter_model_type
                    if mt not in type_models:
                        mt = list(type_models.keys())[0]
                    m_info = type_models[mt]
                    X = np.array(features + [dir_num]).reshape(1, -1)
                    X_s = m_info['scaler'].transform(X)
                    if hasattr(m_info['model'], 'predict_proba'):
                        prob = m_info['model'].predict_proba(X_s)[0]
                        wp = float(prob[1]) if len(prob) > 1 else 0.5
                    else:
                        wp = float(m_info['model'].predict(X_s)[0])
                    if wp >= CH_THRESHOLD:
                        ch_go = True

            # Vote: 1=Both, 2=SMA, 3=CH, 0=Skip
            if sma_go and ch_go:
                algo_mode = 1
            elif sma_go:
                algo_mode = 2
            elif ch_go:
                algo_mode = 3
            else:
                algo_mode = 0

            return jsonify({
                "algo_mode": algo_mode,
                "sma_go": sma_go,
                "ch_go": ch_go
            })
        except Exception as e:
            return jsonify({"algo_mode": 1, "sma_go": True, "ch_go": True, "error": str(e)})

    @app.route('/retrain', methods=['POST'])
    def api_retrain():
        return jsonify({"status": "use 'train' mode to retrain"})

    @app.route('/health', methods=['GET'])
    def health():
        return jsonify({
            "status": "ok",
            "version": "experiment",
            "model_type": model_type,
            "param_models": param_model_names,
            "filter_types": list(all_filter_results.keys())
        })

    print(f"\n=== EXPERIMENT SERVER [{model_type}] on port {port} ===")
    print(f"POST /predict  -> 12 params")
    print(f"POST /filter   -> GO/SKIP")
    print(f"GET  /health   -> status\n")
    app.run(host='127.0.0.1', port=port, debug=False)


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage:")
        print("  python ml_server_experiment.py train")
        print("  python ml_server_experiment.py serve [xgb|rf|mlp|ridge|gbr|ensemble]")
        sys.exit(1)

    mode = sys.argv[1]
    if mode == "train":
        train_all()
    elif mode == "serve":
        mt = sys.argv[2] if len(sys.argv) > 2 else "xgb"
        serve(model_type=mt)
    else:
        print(f"Unknown mode: {mode}")
