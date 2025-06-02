#include <reg51.h>
#include <stdio.h>

// 定義 LCD 的 I2C 裝置地址
#define LCD_ADDR 0x4E

// === 硬體腳位定義 ===
// I2C 腳位（SDA = 資料線, SCL = 時脈線）
sbit SDA = P3^1;
sbit SCL = P3^0;

// DS18B20 資料線
sbit DQ  = P1^1;

// 按鈕定義（調整上下限用）
sbit KEY1 = P2^0;  // 下限 -1
sbit KEY2 = P2^1;  // 下限 +1
sbit KEY3 = P3^2;  // 上限 -1
sbit KEY4 = P3^3;  // 上限 +1

// LED 指示燈
sbit RED_LED   = P1^2;  // 高溫紅燈
sbit GREEN_LED = P1^3;  // 低溫綠燈

// === I2C 通訊函式 ===
void I2C_Delay() {
    unsigned char i;
    for(i=0; i<10; i++);  // 簡單延遲
}

void I2C_Start() {
    SDA = 1; SCL = 1; I2C_Delay();
    SDA = 0; I2C_Delay();
    SCL = 0;
}

void I2C_Stop() {
    SDA = 0; SCL = 1; I2C_Delay();
    SDA = 1; I2C_Delay();
}

// 傳送一個 byte 資料
void I2C_Write(unsigned char dat) {
    unsigned char i;
    for(i=0; i<8; i++) {
        SDA = (dat & 0x80);  // 傳送最高位元
        SCL = 1; I2C_Delay();
        SCL = 0;
        dat <<= 1;
    }
    SDA = 1;  // 回應位元
    SCL = 1; I2C_Delay();
    SCL = 0;
}

// === LCD 函式 ===
// 傳送 LCD 命令
void LCD_WriteCmd(unsigned char cmd) {
    unsigned char high = cmd & 0xF0;
    unsigned char low = (cmd << 4) & 0xF0;
    I2C_Start(); I2C_Write(LCD_ADDR);
    I2C_Write(high | 0x0C); I2C_Write(high | 0x08);
    I2C_Write(low  | 0x0C); I2C_Write(low  | 0x08);
    I2C_Stop();
}

// 傳送 LCD 顯示資料
void LCD_WriteData(unsigned char dat) {
    unsigned char high = dat & 0xF0;
    unsigned char low = (dat << 4) & 0xF0;
    I2C_Start(); I2C_Write(LCD_ADDR);
    I2C_Write(high | 0x0D); I2C_Write(high | 0x09);
    I2C_Write(low  | 0x0D); I2C_Write(low  | 0x09);
    I2C_Stop();
}

// 設定游標位置（row: 0 或 1）
void LCD_SetCursor(unsigned char row, unsigned char col) {
    LCD_WriteCmd((row == 0) ? 0x80 + col : 0xC0 + col);
}

// 輸出一串字元到 LCD
void LCD_Print(char *str) {
    while(*str) LCD_WriteData(*str++);
}

// 初始化 LCD
void LCD_Init() {
    LCD_WriteCmd(0x33); LCD_WriteCmd(0x32);  // 初始化命令
    LCD_WriteCmd(0x28);                      // 4位元, 2行
    LCD_WriteCmd(0x0C);                      // 顯示開啟, 不顯示游標
    LCD_WriteCmd(0x06);                      // 文字方向左至右
    LCD_WriteCmd(0x01);                      // 清除畫面
}

// === DS18B20 溫度感測 ===
void DS_Delay(unsigned int i) {
    while(i--);  // 延遲
}

// 發出重置訊號
void DS_Reset() {
    DQ = 0; DS_Delay(600);  // 拉低
    DQ = 1; DS_Delay(60);   // 放高
}

// 傳送一個 byte 給 DS18B20
void DS_WriteByte(unsigned char dat) {
    unsigned char i;
    for(i=0; i<8; i++) {
        DQ = 0;
        DQ = dat & 0x01;
        DS_Delay(10);
        DQ = 1;
        dat >>= 1;
    }
}

// 從 DS18B20 接收一個 byte
unsigned char DS_ReadByte() {
    unsigned char i, dat = 0;
    for(i=0; i<8; i++) {
        DQ = 0;
        dat >>= 1;
        DQ = 1;
        if(DQ) dat |= 0x80;
        DS_Delay(10);
    }
    return dat;
}

// 讀取溫度值，單位為 0.1°C
int DS_ReadTemperature() {
    unsigned char tempL, tempH;
    int temp;
    DS_Reset(); DS_WriteByte(0xCC); DS_WriteByte(0x44);  // 啟動轉換
    DS_Delay(10000);  // 等待轉換完成
    DS_Reset(); DS_WriteByte(0xCC); DS_WriteByte(0xBE);  // 讀取資料
    tempL = DS_ReadByte();
    tempH = DS_ReadByte();
    temp = (tempH << 8) | tempL;
    return temp * 0.625;  // 轉成 0.1°C 單位
}

// === 按鈕設定上下限 ===
void Check_Keys(int *lo, int *hi) {
    // 下限 -1
    if(KEY1 == 0) {
        DS_Delay(5000);
        if(KEY1 == 0) (*lo)--;
        while(KEY1 == 0);
    }

    // 下限 +1
    if(KEY2 == 0) {
        DS_Delay(5000);
        if(KEY2 == 0) (*lo)++;
        while(KEY2 == 0);
    }

    // 上限 -1
    if(KEY3 == 0) {
        DS_Delay(5000);
        if(KEY3 == 0) (*hi)--;
        while(KEY3 == 0);
    }

    // 上限 +1
    if(KEY4 == 0) {
        DS_Delay(5000);
        if(KEY4 == 0) (*hi)++;
        while(KEY4 == 0);
    }

    // 保持上下限距離合理（至少差 1 度）
    if(*hi < *lo + 1) *hi = *lo + 1;
    if(*lo > *hi - 1) *lo = *hi - 1;
}

// === 溫度 LED 顯示控制 ===
void Update_LED(int temp, int lo, int hi) {
    if(temp > hi * 10) {
        RED_LED = 0;     // 高溫亮紅燈
        GREEN_LED = 1;
    } else if(temp < lo * 10) {
        RED_LED = 1;
        GREEN_LED = 0;   // 低溫亮綠燈
    } else {
        RED_LED = 1;
        GREEN_LED = 1;   // 正常範圍不亮燈
    }
}

// === 主程式 ===
void main() {
    int temp;               // 當前溫度（0.1°C）
    int temp_hi = 30;       // 預設上限溫度
    int temp_lo = 20;       // 預設下限溫度
    char line1[16], line2[16];  // LCD 顯示內容

    LCD_Init();             // 初始化 LCD
    RED_LED = 1;
    GREEN_LED = 1;

    while(1) {
        temp = DS_ReadTemperature();  // 讀取溫度

        // 顯示溫度
        sprintf(line1, "Temp:%2d.%d C  ", temp / 10, temp % 10);
        LCD_SetCursor(0, 0);
        LCD_Print(line1);

        // 顯示上下限
        sprintf(line2, "Lo:%2d Hi:%2d   ", temp_lo, temp_hi);
        LCD_SetCursor(1, 0);
        LCD_Print(line2);

        // 掃描按鈕
        Check_Keys(&temp_lo, &temp_hi);

        // 控制 LED
        Update_LED(temp, temp_lo, temp_hi);
    }
}
