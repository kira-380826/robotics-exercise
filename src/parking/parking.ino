#include <Adafruit_MCP3008.h>
#include <Servo.h>

// --- ハードウェア定義 ---
Adafruit_MCP3008 adc;
Servo servoL;
Servo servoR;
const int PIN_MOTOR_L = 5;
const int PIN_MOTOR_R = 4;

// --- 追加: 空車判定と状態遷移用 ---
const int PIN_IR_FRONT = A5;

// 【チューニングポイント1: 空車判定の閾値】
// 300未満なら「壁が遠い＝空車」、300以上なら「障害物が近い＝満車」と判定。
// 実機でテストし、空車時のセンサ値と満車時のセンサ値の中間くらいに設定してください。
const int EMPTY_THRESHOLD = 300;

enum State {
  STATE_TRACE,
  STATE_CHECK_INSIDE,
  STATE_CHECK_OUTSIDE,
  STATE_RETURN_LINE,
  STATE_PARKING,
  STATE_LEAVE_PARKING, // 追加: 駐車枠から出る状態
  STATE_DONE
};
State currentState = STATE_TRACE;

// --- 調整用パラメータ（実機で後から変更する部分） ---
// ベース速度（90が停止。左は大きく、右は小さくすると前進する）
int baseSpeedL = 143; 
int baseSpeedR = 34;  
// アライメント（その場旋回）専用のPゲイン
float Kp_align = 0.10;

// PI制御のゲイン
float Kp = 0.04; 
float Ki = 0.01; 
int CROSS_THRESHOLD = 3000; 

// --- グローバル変数 ---
int v[6] = {0, 0, 0, 0, 0, 0}; // 6連センサの値
long integral = 0;             // 積分値（I制御用）
int targetSpotValue = 0;       // 空車判定時の赤外線センサ値（距離推定用）
bool parkedInside = true;      // 内側・外側どちらに駐車したかの記憶用

void setup() {
  Serial.begin(9600);
  adc.begin();
  
  servoL.attach(PIN_MOTOR_L);
  servoR.attach(PIN_MOTOR_R);
  
  // モータ初期化（停止）
  servoL.write(93);
  servoR.write(91);
  
  delay(2000); // 起動待ち
}

// ==========================================
// レイヤ1: ハードウェア抽象化 (HAL)
// ==========================================

void readLineSensors() {
  for (int i = 0; i < 6; i++) {
    v[i] = adc.readADC(i);
  }
}

void setMotors(int valL, int valR) {
  if(valL > 180) valL = 180;
  if(valL < 0)   valL = 0;
  if(valR > 180) valR = 180;
  if(valR < 0)   valR = 0;
  
  servoL.write(valL);
  servoR.write(valR);
}

// ==========================================
// レイヤ2: 制御ロジック
// ==========================================

int calculateError() {
  int error = (-3 * v[0]) + (-2 * v[1]) + (-1 * v[2]) 
            + ( 1 * v[3]) + ( 2 * v[4]) + ( 3 * v[5]);
  return error;
}

void traceLinePI() {
  readLineSensors();
  
  int error = calculateError();
  
  integral = integral + error;

  if(integral>1000){
    integral = 1000;
  }else if(integral<-1000){
    integral = -1000;
  }
  
  int turn = (error * Kp) + (integral * Ki);
  
  int outL = baseSpeedL + turn;
  int outR = baseSpeedR + turn; 
  
  setMotors(outL, outR);
}

void alignToLine() {
  Serial.println("Start Alignment...");
  
  while (true) {
    readLineSensors();
    int error = calculateError();
    
    // 【オプション】真っ白な床での誤判定（暴走）を防ぐための追加条件
    // もし旋回後にラインを見失ってあらぬ方向へ暴走する場合は、以下のブロックのコメントを外し、
    // 下にある【デフォルト】の if (abs(error) < 5) ブロックを消してみてください。
    /*
    bool onLine = (v[2] > 200 || v[3] > 200); // 中央付近が黒線を踏んでいるか
    if (abs(error) < 5 && onLine) {
      setMotors(93, 91); // 停止
      Serial.println("Alignment Complete!");
      break; 
    }
    */
    
    // 【デフォルト】誤差が少なければ補正完了とする（parking_v2.inoで実績あり）
    if (abs(error) < 5) {
      setMotors(93, 91); // 停止
      Serial.println("Alignment Complete!");
      break; 
    }
    
    int turnSpeed = error * Kp_align;
    
    if (turnSpeed > 30) turnSpeed = 30;
    if (turnSpeed < -30) turnSpeed = -30;
    
    setMotors(93 + turnSpeed, 91 + turnSpeed);
    
    delay(75);
  }
}

// 【チューニングポイント4: 旋回角度の調整】
// 電池残量やモータの個体差によって旋回スピードが変わります。
// 90度より回りすぎる場合は delay(530) を小さく、足りない場合は大きくしてください。
void turn90Right() {
  setMotors(150, 150); 
  delay(530); // ←ここの時間を調整
  setMotors(93, 91);
  delay(100);
  setMotors(98,88);
  delay(500);
}

void turn90Left() {
  setMotors(30, 30); 
  delay(530); // ←右回転と同じように調整
  setMotors(93, 91);
  delay(100);
  setMotors(98,88);
  delay(500);
}

// 180度旋回は90度旋回の約2倍の時間を設定しています。実機を見て微調整してください。
void turn180() {
  setMotors(150, 150); 
  delay(1060); // ←ここの時間を調整
  setMotors(93, 91);
  delay(100);
  setMotors(98,88);
  delay(500);
}

bool isEmptySpot() {
  int val = analogRead(PIN_IR_FRONT);
  Serial.print("Front IR: ");
  Serial.println(val);
  return (val < EMPTY_THRESHOLD);
}

// ==========================================
// レイヤ3: 意思決定レイヤ (Decision Making)
// ==========================================

bool isCrossroad() {
  int sum = 0;
  for (int i = 0; i < 6; i++) {
    sum += v[i];
  }
  
  if (sum > CROSS_THRESHOLD) {
    return true;
  } else {
    return false;
  }
}

// ==========================================
// レイヤ4: メインループ
// ==========================================
void loop() {
  readLineSensors(); // 毎ループ最初に1回だけセンサを読む

  switch (currentState) {
    case STATE_TRACE:
      if (isCrossroad() == true) {
        setMotors(93, 91);
        delay(200);
        setMotors(98,88);
        delay(100);
        Serial.println("Crossroad Detected! STOP.");
        delay(1000);
        currentState = STATE_CHECK_INSIDE;
      } else {
        traceLinePI();
      }
      break;

    case STATE_CHECK_INSIDE:
      Serial.println("Checking Inside...");
      turn90Right();
      alignToLine();
      if (isEmptySpot()) {
        targetSpotValue = analogRead(PIN_IR_FRONT); // 旋回前の値を記録
        parkedInside = true;  // 内側に駐車したことを記憶
        currentState = STATE_PARKING;
      } else {
        currentState = STATE_CHECK_OUTSIDE;
      }
      break;

    case STATE_CHECK_OUTSIDE:
      Serial.println("Checking Outside...");
      turn180();
      alignToLine();
      if (isEmptySpot()) {
        targetSpotValue = analogRead(PIN_IR_FRONT); // 旋回前の値を記録
        parkedInside = false; // 外側に駐車したことを記憶
        currentState = STATE_PARKING;
      } else {
        currentState = STATE_RETURN_LINE;
      }
      break;

    case STATE_RETURN_LINE:
      Serial.println("Return to Main Line...");
      turn90Right(); // 外側（左）を向いている状態から右に90度回れば元の進行方向を向く
      alignToLine();
      
      // 不感時間（十字マークを再び交差点と誤認しないため強制前進）
      setMotors(baseSpeedL, baseSpeedR);
      delay(500);
      
      currentState = STATE_TRACE;
      break;

    case STATE_PARKING:
      Serial.println("Empty Spot Found! PARKING...");
      
      { // 変数宣言用のスコープ
        // 1. センサ値 V から壁までの距離 D (mm) を近似式で算出
        int V = targetSpotValue;
        float D = 0.00224 * V * V - 2.2585 * V + 654.5;
        
        // 2. バックすべき距離（mm）を計算
        // 【チューニングポイント2: バック距離の微調整】
        // バックしすぎて壁にぶつかる場合 -> OFFSET_MM を大きくする (例: 210.0)
        // バックが足りず隙間が空く場合   -> OFFSET_MM を小さくする (例: 170.0)
        float OFFSET_MM = 190.0; // 170(ロボット長) + 20(壁との隙間) 等
        float targetDistance = D - OFFSET_MM;
        
        // 3. 距離(mm)をバックする時間(ms)に変換する係数
        // 【チューニングポイント3: バック時間の係数】
        // モータのスピード（電池残量）によって進む距離が変わります。
        // 計算距離は合っているのにバック距離が短い場合 -> TIME_PER_MM を大きくする (例: 12.0)
        // バック距離が長すぎる場合 -> TIME_PER_MM を小さくする (例: 8.0)
        float TIME_PER_MM = 10.0; // 100mm進むのに1000msかかる場合など
        int backTimeMs = targetDistance * TIME_PER_MM;
        
        if (backTimeMs < 0) backTimeMs = 0;
        
        Serial.print("Target Dist(mm): ");
        Serial.println(targetDistance);
        Serial.print("Back Time(ms): ");
        Serial.println(backTimeMs);
        
        // 4. まず壁に背を向けるために180度旋回
        turn180();
        alignToLine();
        
        // 5. バックの実行 (ベース速度を反転させて後退)
        int reverseL = 180 - baseSpeedL; // 例: 180 - 143 = 37
        int reverseR = 180 - baseSpeedR; // 例: 180 - 34 = 146
        setMotors(reverseL, reverseR);
        delay(backTimeMs);
        
        setMotors(93, 91); // 停止
        Serial.println("Parking Completed.");
        
        // 駐車後、3秒間停止してアピール
        delay(3000); 
      }
      
      currentState = STATE_LEAVE_PARKING;
      break;

    case STATE_LEAVE_PARKING:
      Serial.println("Leaving Parking Spot...");
      
      // 【チューニングポイント5: 駐車枠からの脱出】
      // 駐車枠内に線がある場合は traceLinePI() を使う手もありますが、
      // ここではシンプルに前進して交差点（メインライン）を探します。

      /*
      // =============================================================
      // 【オプション】駐車枠内のラインを使ってまっすぐに復帰する場合
      // もし脱出時に斜めにズレてしまう場合は、このブロックのコメントを外し、
      // 下の【デフォルト】の setMotors(baseSpeedL, baseSpeedR); を消してください。
      
      setMotors(baseSpeedL, baseSpeedR); // ラインを探しながら前進
      if (v[2] > 500 || v[3] > 500) {    // 中央のセンサがラインを捉えたら
        setMotors(93, 91); delay(100);
        alignToLine();                   // ラインに対して真っ直ぐに補正
        
        // 補正後、交差点が見つかるまでライントレースで進む
        while (isCrossroad() == false) {
          traceLinePI();
          delay(10);
        }
      }
      // =============================================================
      */
      
      // 【デフォルト】単純に前進して交差点を探す
      setMotors(baseSpeedL, baseSpeedR);
      
      if (isCrossroad()) {
        // メインラインに到達
        setMotors(93, 91);
        delay(500);
        
        if (parkedInside) {
          // 右側の枠から出てきた = 現在左を向いている
          // メインラインの進行方向を向くには右に90度
          turn90Right();
        } else {
          // 左側の枠から出てきた = 現在右を向いている
          // メインラインの進行方向を向くには左に90度
          turn90Left();
        }
        alignToLine();
        
        // 交差点を完全に通り抜けるための不感時間
        setMotors(baseSpeedL, baseSpeedR);
        delay(500);
        
        currentState = STATE_TRACE; // 再び探索の旅へ
      }
      break;

    case STATE_DONE:
      setMotors(93, 91);
      while(1) { delay(100); }
      break;
  }
  
  delay(10); 
}