//アームsensor sample
/*アームの距離センサがある程度障害物に反応している間停止し、
それ以外のときは直進する*/
#include <Servo.h>
Servo servoR;
Servo servoL;
Servo arm;
int a; // arm sensor
void setup() {
  // put your setup code here, to run once:

servoR.attach(4);//右モータのピンが4に配線されている場合のアタッチ
servoL.attach(5);
arm.attach(8);//ピン8番に刺さっているモータをアームのモータとする
//各モータの回転位置の初期化
servoR.write(90);
servoL.write(90);
arm.write(90);
Serial.begin(9600);//シリアル通信の転送速度
}

void loop(){
  a = analogRead(A5);//距離センサ読み取り
  Serial.println(a);//センサ値出力
  Serial.println();//シリアルモニタ上で改行
if(a>500) {//停止（前に障害物があるかの閾値を仮に500とした場合）
  servoL.write(90);;//モータの個体差によっては90では止まらない場合もある
  servoR.write(90);
  arm.write(90);
}else{//それ以外のとき直進速度あたえる
  servoL.write(150);//値91~180で前向き　指定角度が変わればモーター回転速度も変わるため
  servoR.write(30);//値0~89で前向き
  arm.write(90);
}
  delay(100);
}
