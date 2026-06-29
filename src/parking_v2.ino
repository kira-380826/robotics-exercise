#include <Adafruit_MCP3008.h>
#include <Servo.h>

// --- ハードウェア定義 ---
Adafruit_MCP3008 adc; //下部センサー
Servo servoL;
Servo servoR;
const int PIN_MOTOR_L = 5;
const int PIN_MOTOR_R = 4;

// --- 調整用パラメータ（実機で後から変更する部分） ---
// ベース速度（90が停止。左は大きく、右は小さくすると前進する）
int baseSpeedL = 143; 
int baseSpeedR = 34;  
// アライメント（その場旋回）専用のPゲイン
// 前進時のKpとは最適な値が異なるため、独立させます
float Kp_align = 0.10;

// PI制御のゲイン（最初はKpだけ調整し、Kiは0にしておくのが定石です）
float Kp = 0.04; //比例ゲイン(P制御)：今のズレを直すパラメータ
float Ki = 0.01; //積分ゲイン(I制御)：過去のズレの蓄積を直すパラメータ
int CROSS_THRESHOLD = 3000; // 6個のセンサ値の合計がこれを超えたら交差点と判定

// --- グローバル変数 ---
int v[6] = {0, 0, 0, 0, 0, 0}; // 6連センサの値
long integral = 0;             // 積分値（I制御用）

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

// センサ値を読み取る関数
void readLineSensors() {
  for (int i = 0; i < 6; i++) {
    v[i] = adc.readADC(i);
  }
}

// モータに値を出力する関数（安全のためのリミッタ付き）
void setMotors(int valL, int valR) {
  // サーボの入力範囲(0〜180)を超えないように制限
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

// 加重平均による偏差(Error)の計算
int calculateError() {
  // 左側のセンサが黒を踏んだらマイナス、右側ならプラスの誤差を出力する
  // (実機のセンサの並び順に合わせて符号は逆転させる必要があるかもしれません)
  int error = (-3 * v[0]) + (-2 * v[1]) + (-1 * v[2]) 
            + ( 1 * v[3]) + ( 2 * v[4]) + ( 3 * v[5]);
  return error;
}

// PI制御によるライントレース実行関数
void traceLinePI() {
  // ※ここでreadLineSensors()を呼ぶと、後でisCrossroad()を呼ぶときに
  // センサを2回読んでしまうので、読み取り処理はloop()側に移動させるのがベストです。
  // (今回は現状のままにしておきます)
  readLineSensors();
  
  int error = calculateError(); //右側に傾いているときはerrorがマイナス
  
  // 積分項の更新（エラーの蓄積）
  integral = integral + error;

  if(integral>1000){
    integral = 1000;
  }else if(integral<-1000){
    integral = -1000;
  }
  
  // 操作量（turn）の計算
  int turn = (error * Kp) + (integral * Ki);
  
  // モータ出力の計算
  // 左右のモータは鏡合わせに配置されているため、両方に「+turn」するだけで
  // 片方は加速、片方は減速となり、自動的に差動旋回になります。
  int outL = baseSpeedL + turn;
  int outR = baseSpeedR + turn; 
  
  setMotors(outL, outR);
}


// 【追加・解除箇所D】アライメント（姿勢補正）関数
// (ステップ4でコメントアウトを解除します)
void alignToLine() {
  Serial.println("Start Alignment...");
  
  while (true) {
    readLineSensors();
    int error = calculateError();
    
    // 誤差が「許容範囲（例: -10 〜 10）」に収まったら補正完了とみなしてループを抜ける
    if (abs(error) < 5) {
      setMotors(93, 91); // 停止
      Serial.println("Alignment Complete!");
      break; 
    }
    
    // アライメント用のP制御（その場旋回）
    int turnSpeed = error * Kp_align;
    
    // リミッタ（旋回スピードが速すぎると線を通り過ぎてしまうため）
    if (turnSpeed > 30) turnSpeed = 30;
    if (turnSpeed < -30) turnSpeed = -30;
    
    // BEATLEの仕様：左右同値（150, 150等）で右回転、（30, 30等）で左回転
    setMotors(93 + turnSpeed, 91 + turnSpeed);
    
    delay(75);
  }
}



// 【追加・解除箇所E】テスト用の旋回関数
// (ステップ4でコメントアウトを解除します)
void turn90Right() {
  // 150, 150 は右回転 [cite: 3]
  setMotors(150, 150); 
  delay(530); // 90度くらい回る適当な時間（後で実機で調整）
  setMotors(93, 91);
  delay(100);
  setMotors(98,88);
  delay(500);
}


// ==========================================
// レイヤ3: 意思決定レイヤ (Decision Making)
// ==========================================

 // 【追加・解除箇所B】交差点判定関数
// (ステップ3でコメントアウトを解除します)
bool isCrossroad() {
  int sum = 0;
  // 6つのセンサ値の合計を計算
  for (int i = 0; i < 6; i++) {
    sum += v[i];
  }
  
  // デバッグ用：シリアルモニタで合計値を確認
  // Serial.print("Sensor Sum: ");
  // Serial.println(sum);

  // 合計値が閾値を超えていたら「交差点」と判定(trueを返す)
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
  
  // 【追加・解除箇所C】交差点で停止するメインループ処理
  // (ステップ3でこのブロックのコメントアウトを解除し、元のtraceLinePI()を消します)
  
  readLineSensors(); // 毎ループ最初に1回だけセンサを読む

  if (isCrossroad() == true) {
    // 交差点を検知したらモータを停止(90)して、無限ループで待機
    setMotors(93, 91);
    delay(200);
    setMotors(98,88);
    delay(100);
    Serial.println("Crossroad Detected! STOP.");
    delay(1000);
    turn90Right();
    alignToLine();
    
    while(1) {
      delay(100); // 完全に停止したまま動かない
    }
  } else {
    // 交差点でなければライントレースを続ける
    traceLinePI();
  }
  

  // --- ステップ1（現在）の処理 ---
  // 上記のコメントを外した時は、以下の1行は削除（またはコメントアウト）してください

  
  // ひたすらライントレースを続ける
  //traceLinePI();
  
  // 制御周期（10ms〜20ms程度が一般的です）
  delay(10); 
}
