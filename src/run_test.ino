//forward,back,left,right,stop motor sample
#include <Adafruit_MCP3008.h>
#include <Servo.h>
Servo servoL;
Servo servoR;
void setup() {
  // put your setup code here, to run once:
servoR.attach(4);//右モータのピンが4に配線されている場合のアタッチ
servoL.attach(5);//左モータのピンが5に配線されている場合のアタッチ
/*モータ回転位置初期化*/
servoR.write(90);
servoL.write(90);
Serial.begin(9600);//シリアル通信の転送速度
}

/*モータの回転角（速度）を与える関数を作っている*/
void set_motor(int L, int R){
  servoL.write(L);//0≦L≦180、90から離れるほど速く回転
  servoR.write(R);//0≦R≦180
}

void loop() {
  // put your main code here, to run repeatedly:
  /*2sごとに直進、右回転、左回転、バック、停止を繰り返す
  上手に動かなければ個体ごとに値を変更する必要あり*/
  set_motor(150,30);//直進
  delay(2000);//2秒プログラム止めて直進を維持
  set_motor(150,150);//右回転
  delay(2000);
  set_motor(30,30);//左回転
  delay(2000);
  set_motor(30,150);//後退
  delay(2000);
  set_motor(90,90);//停止
  delay(2000);
}

/*↓のような書き方でも同様の動作が可能*/
/*
void back(){
  servoL.write(30);//0~89
  servoR.write(150);//91~180
}
void forward(){
  servoL.write(150);//91~180
  servoR.write(30);//0~89
} 
void right(){
  servoL.write(150);
  servoR.write(150);
}
void left(){
  servoL.write(30);
  servoR.write(30);
  }

void stoppp(){
  servoL.write(90);
  servoR.write(90);
  }
*/
/*
void loop() {
  // put your main code here, to run repeatedly:
  forward();
  delay(2000);
  right();
  delay(2000);
  left();
  delay(2000);
  back();
  delay(2000);
  stoppp();
  delay(2000);
}*/
