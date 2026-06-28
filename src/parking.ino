#include <Adafruit_MCP3008.h>
#include <Servo.h>

// --- ハードウェア定義 ---
Adafruit_MCP3008 adc;
Servo servoL;
Servo servoR;
const int PIN_MOTOR_L = 5;
const int PIN_MOTOR_R = 4;

// --- 調整用パラメータ（実機で後から変更する部分） ---
// ベース速度（90が停止。左は大きく、右は小さくすると前進する）
int baseSpeedL = 150; 
int baseSpeedR = 30;  

// PI制御のゲイン（最初はKpだけ調整し、Kiは0にしておくのが定石です）
float Kp = 0.05; //比例ゲイン(P制御)：今のズレを直すパラメータ
float Ki = 0.00; //積分ゲイン(I制御)：過去のズレの蓄積を直すパラメータ

// --- グローバル変数 ---
int v[6] = {0, 0, 0, 0, 0, 0}; // 6連センサの値
long integral = 0;             // 積分値（I制御用）

void setup() {
  Serial.begin(9600);
  adc.begin();
  
  servoL.attach(PIN_MOTOR_L);
  servoR.attach(PIN_MOTOR_R);
  
  // モータ初期化（停止）
  servoL.write(90);
  servoR.write(90);
  
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
  readLineSensors();
  
  int error = calculateError();
  
  // 積分項の更新（エラーの蓄積）
  integral = integral + error;
  
  // 操作量（turn）の計算
  int turn = (error * Kp) + (integral * Ki);
  
  // モータ出力の計算
  // 左右のモータは鏡合わせに配置されているため、両方に「+turn」するだけで
  // 片方は加速、片方は減速となり、自動的に差動旋回になります。
  int outL = baseSpeedL + turn;
  int outR = baseSpeedR + turn; 
  
  setMotors(outL, outR);
}

// ==========================================
// レイヤ4: メインループ
// ==========================================
void loop() {
  // ひたすらライントレースを続ける
  traceLinePI();
  
  // 制御周期（10ms〜20ms程度が一般的です）
  delay(10); 
}