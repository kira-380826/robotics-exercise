# --- fit_sensor.plt ---
# グラフの見た目と出力設定
set title "IR Sensor Calibration (Fractional Model)"
set xlabel "Distance (mm)"
set ylabel "Sensor Value"
set grid

# 近接ノイズを除外する範囲設定
set xrange [50:300]

# 近似関数の定義
f(x) = A / (x + B) + C

# パラメータの初期値設定
A = 30000
B = 10
C = 0

# 最小二乗法の実行
fit [50:*] f(x) 'fixed_data.txt' using 1:2 via A, B, C

# グラフの描画
plot 'fixed_data.txt' using 1:2 with points pt 7 title "Raw Data", \
     f(x) with lines linewidth 2 title "Fitted Curve"

# 描画後、ウィンドウを閉じないようにするコマンド
pause -1 "Press Enter or Return to close the graph..."