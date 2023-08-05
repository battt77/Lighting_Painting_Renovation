#include <Wire.h> //IIC
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <string.h>

# define White 14 //GPIO0 D3
# define Yellow 12 //GPIO2 D4

#define  TOTAL_KEY  13 
#define  MODE_KEY  15
#define  ADD_KEY   16
#define  SUB_KEY   2

#define MODE_KEY_VAL digitalRead(MODE_KEY)  //PULL_DOMN
#define ADD_KEY_VAL digitalRead(ADD_KEY)    //PULL_UP
#define SUB_KEY_VAL digitalRead(SUB_KEY)    //PULL_UP

//Wifi message
#ifndef STASSID
#define STASSID "ESP8266"
#define STAPSK  "00000000"
#endif

const char* ssid = STASSID;
const char* password = STAPSK;

uint8_t led_mode=2; //0:White,1:Yellow,2:White & Yellow
uint8_t pwm_mode=0; //0:AUTO,1:HAND
uint8_t led_switch=1; //0:off,1:on
int hand_mode_pwm_num=500;//the pwm num in hand mode
float pwm_num=0;//0~1000
float coefficient=-5;

int BH1750address = 0x23;
byte buff[2];
float lx_val = 0;

WiFiServer server(80);

uint8_t CONTORL_MODE=1;//0:hand,1:auto

void setup()
{
  Wire.begin();
  BH1750_Init(BH1750address);
  Serial.begin(115200);

  //PWM pin
  pinMode(White,OUTPUT);
  pinMode(Yellow,OUTPUT);
  
  //AP mode set
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.println(IP);

  //key pin
  pinMode(TOTAL_KEY, INPUT);
  pinMode(MODE_KEY, INPUT);
  pinMode(ADD_KEY, INPUT);
  pinMode(SUB_KEY, INPUT);

  server.begin();

  analogWriteFreq(1000);
  analogWriteRange(1000);
}

void loop()
{
  uint8_t key_val=0;
  
  CONTORL_MODE=digitalRead(TOTAL_KEY);
  web_server();
  
  // parameter generation
  if(CONTORL_MODE)
  {
      if (2 == BH1750_Read(BH1750address))
    {
      if (buff[0] == 255 && buff[1] == 255)
    {
      lx_val = 65535;
    } else {
      lx_val = ((buff[0] << 8) | buff[1]) / 1.2; //芯片手册中规定的数值计算方式
    }
    Serial.print(lx_val, DEC);
    Serial.println("[lx]");
    }
    pwm_cal();
  }
  else
  {
    Serial.print(hand_mode_pwm_num);
    key_val=key_scan();
    switch(key_val)
    {
      case 1:
        if(led_mode==2)
        {
          led_mode=0;
        }
        else
        {
          led_mode++;
        }
      break;
      case 2:
        if(hand_mode_pwm_num<1000)
        {
          hand_mode_pwm_num+=100;
        }
        else
        {
          hand_mode_pwm_num=1000;
        }
      break;
      case 3:
        if(hand_mode_pwm_num>0)
      {
        hand_mode_pwm_num-=100;
      }
        else
      {
        hand_mode_pwm_num=0;
      }
      break;
      default:
      break;
    }
}


  //pwm output control
  if(led_switch||!CONTORL_MODE)
  {
  switch(led_mode)
  {
    case 0:
      analogWrite(Yellow,0);
      if(pwm_mode||!CONTORL_MODE)
      {
        analogWrite(White,hand_mode_pwm_num);
      }
      else
      {
        analogWrite(White,pwm_num);
      }
      break;
    case 1:
      analogWrite(White,0);
      if(pwm_mode||!CONTORL_MODE)
      {
        analogWrite(Yellow,hand_mode_pwm_num);
      }
      else
      {
        analogWrite(Yellow,pwm_num);
      }
      break;
    case 2:
      if(pwm_mode||!CONTORL_MODE)
      {
        analogWrite(White,hand_mode_pwm_num);
        analogWrite(Yellow,hand_mode_pwm_num);
      }
      else
      {
        analogWrite(White,pwm_num);
        analogWrite(Yellow,pwm_num);  
      }
      break;
    default:
      analogWrite(Yellow,0);
      if(pwm_mode||!CONTORL_MODE)
      {
        analogWrite(White,hand_mode_pwm_num);
      }
      else
      {
        analogWrite(White,pwm_num);
      }
      break; 
  }
  }
  else
  {
    analogWrite(White,0);
    analogWrite(Yellow,0);
  }
  

}

uint8_t key_flag=1;
uint8_t key_scan()
{
  if((MODE_KEY_VAL||!ADD_KEY_VAL||!SUB_KEY_VAL)&&key_flag)
  {
    key_flag=0;
    delay(10);
    if(MODE_KEY_VAL) return 1;
    else if(!ADD_KEY_VAL) return 2;
    else if(!SUB_KEY_VAL) return 3;
  }
  else if(!MODE_KEY_VAL&&ADD_KEY_VAL&&SUB_KEY_VAL) key_flag=1;
  return 0;
}


int BH1750_Read(int address) 
{
  int i = 0;
  Wire.beginTransmission(address);
  Wire.requestFrom(address, 2);
  while (Wire.available())
  {
    buff[i] = Wire.read();  // receive one byte
    i++;
  }
  Wire.endTransmission();
  return i;
}

void BH1750_Init(int address)
{
  Wire.beginTransmission(address);
  Wire.write(0x10);
  Wire.endTransmission();
}

void pwm_cal()
{
  if(lx_val>200)
  {
    pwm_num=0;
  }
  else if(lx_val<=0)
  {
    pwm_num=1000;
  }
  else
  {
    pwm_num=coefficient*lx_val+1000;
  }

  if(pwm_num>1000)
  {
    pwm_num=1000;
  }

}

String  led_mode_s="晴朗"; 
String  pwm_mode_s="自动";
String  led_cond_s="关闭";
String  led_cond_opposite_s="开启";
String  led_cond_url_s="http://192.168.4.1/ON";
String  light_adjust_mode_s="(禁用)";
String  button_mode_s="style=\"height:75px;width:175px;font-size:30px;color:grey\"";
void web_server()
{
  WiFiClient client = server.available();
  
  if (client) {

  client.setTimeout(5000); // default is 1000

  // Read the first line of the request
  String req = client.readStringUntil('\r');
  Serial.println(F("request: "));
  Serial.println(req);
  
 
  // Match the request;url content
  if (req.indexOf(F("/WH")) != -1) {
    led_mode = 0;
    //led_mode_s="晴朗";
  } else if (req.indexOf(F("/YL")) != -1) {
    led_mode = 1;
    //led_mode_s="黄昏";
  } 
  else if(req.indexOf(F("/CB")) != -1)
  {
    led_mode=2;
    //led_mode_s="多云";
  }
  else if(req.indexOf(F("/AU")) != -1)
  {
    pwm_mode=0;
    //pwm_mode_s="自动";
  }
  else if(req.indexOf(F("/HA")) != -1)
  {
    pwm_mode=1;
    //pwm_mode_s="手动";
  }
  else if(req.indexOf(F("/SUB")) != -1)
  {
    if(hand_mode_pwm_num>0)
    {
      hand_mode_pwm_num-=100;
    }
    else
    {
      hand_mode_pwm_num=0;
    }
  }
  else if(req.indexOf(F("/ADD")) != -1)
  {
    if(hand_mode_pwm_num<1000)
    {
      hand_mode_pwm_num+=100;
    }
    else
    {
      hand_mode_pwm_num=1000;
    }
  }
  else if(req.indexOf(F("/ON")) != -1)
  {
    led_switch=1;
    //led_cond_s="开启";
    //led_cond_opposite_s="关闭";
    //led_cond_url_s="http://192.168.4.1/OFF";
  }
  else if(req.indexOf(F("/OFF"))!=-1)
  {
    led_switch=0;
    //led_cond_s="关闭";
    //led_cond_opposite_s="开启";
    //led_cond_url_s="http://192.168.4.1/ON";
  }
  else {
    Serial.println(F("invalid request"));
  }

//string judge 
  if(led_switch)
  {
    led_cond_s="开启";
    led_cond_opposite_s="关闭";
    led_cond_url_s="http://192.168.4.1/OFF";
  }
  else
  {
    led_cond_s="关闭";
    led_cond_opposite_s="开启";
    led_cond_url_s="http://192.168.4.1/ON";
  }

  if(!pwm_mode)
  {
    pwm_mode_s="自动";
    light_adjust_mode_s="(禁用)";
    button_mode_s="style=\"height:75px;width:175px;font-size:30px;color:darkgrey\"";
  }
  else
  {
    pwm_mode_s="手动";
    light_adjust_mode_s="(启用)";
    button_mode_s="style=\"height:75px;width:175px;font-size:30px;color:black\"";
  }

  switch(led_mode)
  {
    case 0:
      led_mode_s="晴朗";
      break;
    case 1:
      led_mode_s="黄昏";
      break;
    case 2:
      led_mode_s="多云";
      break;
    default:
    break;
  }

  // read/ignore the rest of the request
  // do not client.flush(): it is for output only, see below
  while (client.available()) {
    // byte by byte is not very efficient
    client.read();
  }

  // Send the response to the client
  // it is OK for multiple small client.print/write,
  // because nagle algorithm will group them into one single packet
String Page=String("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE html>")+
"<html>"+
"<head>"+
"<meta charset=\"utf-8\">"+
"<title>灯光画远程遥控服务器</title>"+
"</head>"+
"<body>"+
"<h1 style=\"text-align:center;font-size:60px;\">欢迎使用灯光画远程遥控服务器</h1>"+
"<hr />"+
"<h2 style=\"text-align:center;font-size:50px;\">总开关</h2>"+
"<p style=\"text-align:center;\">"+
    "<font style=\"font-size:40px\">"+
        "现在的灯光画已经<b style=\"color:red;\">"+led_cond_s+"</b>，点击下列选项可<b>"+led_cond_opposite_s+"</b>灯光画"+
    "</font>"+ 
    "<form style=\"text-align:center;\">"+
        "<input type=\"button\" value=\" "+led_cond_opposite_s+" \" style=\"height:75px;width:175px;font-size:30px;\" onclick=\"window.location.href='"+led_cond_url_s+"'\">"+
    "</form>"+
"</p>"+
"<hr/>"+
"<h2 style=\"text-align:center;font-size:50px;\">光色选择</h2>"+
"<p style=\"text-align:center;\">"+
    "<font style=\"font-size:40px\">"+
        "现在的光色是<b style=\"color:red;\">"+led_mode_s+"</b>，点击下列选项可切换光色"+
    "</font>"+ 
    "<form style=\"text-align:center;\">"+
        "<input type=\"button\" style=\"height:75px;width:175px;font-size:30px;\" value=\"晴朗\" onclick=\"window.location.href='http://192.168.4.1/WH'\">" +
        "<input type=\"button\" style=\"height:75px;width:175px;font-size:30px;\" value=\"黄昏\" onclick=\"window.location.href='http://192.168.4.1/YL'\">" +
        "<input type=\"button\" style=\"height:75px;width:175px;font-size:30px;\" value=\"多云\" onclick=\"window.location.href='http://192.168.4.1/CB'\">" +
    "</form>"+
"</p>"+
"<hr/>"+
"<h2 style=\"text-align:center;font-size:50px;\">调光模式</h2>"+
"<p style=\"text-align:center;\">"+
    "<font style=\"font-size:40px\">"+
        "现在的调光模式是<b style=\"color:red;\">"+pwm_mode_s+"</b>，点击下列选项可切换调光模式"+
    "</font>"+
    "<form style=\"text-align:center;\">"+
      "<input type=\"button\" style=\"height:75px;width:175px;font-size:30px;\" value=\"自动\" onclick=\"window.location.href='http://192.168.4.1/AU'\">"+
      "<input type=\"button\" style=\"height:75px;width:175px;font-size:30px;\" value=\"手动\" onclick=\"window.location.href='http://192.168.4.1/HA'\">"+
    "</form>"+
"</p>"+
"<hr />"+ 
"<h2 style=\"text-align:center;font-size:50px;\">亮度调节"+light_adjust_mode_s+"</h2>"+
"<p style=\"text-align:center;\">"+
    "<font style=\"font-size:40px\">"+
        "现在的亮度是<b style=\"color:red;\">"+String(hand_mode_pwm_num/10)+"%</b>，点击下列选项可调节光亮度"+
    "</font>"+
    "<br>"+
    "<font size=\"2\" style=\"color:red;font-size:30px;\">"+
        "注:此功能在<i>自动调光</i>模式下无效"+
    "</font>"+
    "<form style=\"text-align:center;\">"+
        "<input type=\"button\"" +button_mode_s+ " value=\"-10%\" onclick=\"window.location.href='http://192.168.4.1/SUB'\">"+
        "<input type=\"button\"" +button_mode_s+ " value=\"+10%\" onclick=\"window.location.href='http://192.168.4.1/ADD'\">"+
    "</form>"+
"</p>"+
"<hr/>"+
"<p style=\"color:blueviolet;text-align:center;font-size:20px;\">"+
    "Copyright © Tao"+
"</p>"+
"</html>";
  client.print(Page);
  // The client will actually be *flushed* then disconnected
  // when the function returns and 'client' object is destroyed (out-of-scope)
  // flush = ensure written data are received by the other side
  Serial.println(F("Disconnecting from client"));
  }
}