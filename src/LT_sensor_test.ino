//ライントレース用下部センサ読み取り sample
/*黒線検知したら直進やめて停止するプログラム*/
#include <Adafruit_MCP3008.h>
/*上記ヘッダファイルをインクルードをしないと6連センサを使用できない
  ツール→ライブラリのインクルード→ライブラリの管理から
  ネットワーク経由出インストール，あるいはWebClassのzipをダウンロードして
  Adafruit_MCP3008をインストールする必要がある*/
#include <Servo.h>
Adafruit_MCP3008 adc; //下部センサの定義 
Servo servoR;
Servo servoL;
int v[6]={0,0,0,0,0,0}; //6連センサの値を格納する配列
int b=400;//黒線上であるかとないかの閾値を仮に400とする
int mtrL=0, mtrR=0;
int i;
int flag = 0;
void setup() {
  // put your setup code here, to run once:
servoR.attach(4);//右車輪のモータのピンが4に配線されている場合のアタッチ
servoL.attach(5);//○○○.(i)でi番目のピンのモータを関連づける

/*モーター初期化*/
//引数に90を入れると基本的には車輪のモータは停止する
servoR.write(90); //○○○.write(int)でモータの回転角（速度）変える
servoL.write(90); //値は0～180の間

adc.begin(); //センサを初期化
Serial.begin(9600); //シリアル通信の転送速度

}
void loop(){
  for (i=0;i<6;i++){ //6センサ分、端のセンサから1つずつセンサ値読み取り
    v[i] = adc.readADC(i); // i番目のセンサ値を取得
    Serial.print(v[i]); //センサ値をシリアルモニタに表示
    Serial.print(" "); 
  }
  Serial.println();//シリアルモニタ上に改行を入力
  for(i=0;i<6;i++){
    if(v[i] > b )flag = 1;//黒線の上では止まるフラグを起動
  }
  if(flag == 1){//止まり続ける処理
    mtrL = 90;//個体差によっては90では停止しない場合もある
    mtrR = 90;
  }else{//それ以外のとき直進速度をあたえる
    mtrL = 150;//値91~180で前方向に回転　指定角度が変わればモーター回転速度も変わる
    mtrR = 30;//値0~89で前方向に回転
  }
  servoL.write(mtrL);
  servoR.write(mtrR);
  delay(100);
  /*100msの間ディレイさせて
   次のループまでの時間を設定している
   指定した速度を維持するようにしたり
   シリアルモニタでセンサ値を見れるようにしている*/ 
}
