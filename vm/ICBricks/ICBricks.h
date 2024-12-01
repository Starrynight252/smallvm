/* 该文件创建于2024/10/11
 *  支持 ICBricks功能
 *
 */
#ifndef _ICBricks_H_
#define _ICBricks_H_

#if defined(ICBRICKS)
#include <Arduino.h>


//固件类型
#define ICBRICKS_BOARD_TYPE "ICBricks2.0" 
//固件版本 1.xn  x大版本更新  n修改Bug或者优化
#define FIRMWARE_VERSION 1


/*--------宏定义-脚位-----------*/
/*----LED GIO脚位-----*/
#define ICBRICKS_LED_GIO 23
/*---电源GIO脚位-输出---*/
#define ICBRICKS_POWER_GIO_OUT 12
/*---电源GIO脚位-输入---*/
#define ICBRICKS_POWER_GIO_IN 19
// 内置声音
#define ICBRICKS_BUZZ 25
// 内置声音_关闭
#define ICBRICKS_BUZZ_SHOD 4

/*---按键GIO脚位----*/
//右
#define ICBRICKS_KEY_0 18 
//下
#define ICBRICKS_KEY_1 26 
//左
#define ICBRICKS_KEY_2 27 
//上
#define ICBRICKS_KEY_3 14 
//中
#define ICBRICKS_KEY_4 13  
// 电池
#define ICBRICKS_VBAT_A 32
// 充电检查
#define ICBRICKS_CHARGE 33
// 是否充电-高为充电完成
#define ICBRICKS_FULLY 5
#define ICBRICKS_TYPE_CC 16

// 一半电量
#define HALF_CHARGE 0x7F
// 警告电量
#define WARNING_BATTERY_LEVEL 0x40

// 电池容量最大值
#define BATTERY_CAPACITY_MAX 3300

// 开关机 延时-无堵塞延时时间
#define ICBRICKS_POWER_INTERVAL 600 // 延时 ms
/*----LED 数量-----*/
// #if HARDWARE_VERSION
#define ICBRICKS_LED_NUMS 5

/*----LED 类型-----*/
#define ICBRICKS_LED_TYPE WS2812
/*----LED 颜色顺序-----*/
#define ICBRICKS_LED_COLOR_ORDER GRB

// 16位拆高8位 高
#define SIXTEEN_SPLIT_HIGH_EIGHT_BIT(bit) ((bit >> 8) & 0xff)
// 16位拆底8位  低
#define SIXTEEN_SPLIT_LOW_EIGHT_BIT(bit) (bit & 0xff)
// 8位结合16位
#define EIGHT_BITS_COMBINED_WITH_SIXTEEN_BITS(Hbit, Lbit) ((Hbit << 8) | Lbit)
//extern Music_Playback_Driver StartupTone;

// 进入GP脚本触发次数
#define NNUMBER_GP_TRIGGERS 3

// 进入GP脚本超时
#define ENTER_GP_TIMEOUT 400 //+TRIGGERING_GP_TIME
// 进入GP脚本时间
#define TRIGGERING_GP_TIME 20 // 15


extern volatile uint8_t GpScriptControlState;



bool ICBricks_GetKeyPressStatus(uint8_t pin,bool sound);
// LED初始化
void ICBricks_Initializ();
// 设备开关机
void ICBricks_SwitchMachine_Task(void *pvParameters);
// 获取开关机状态，true 开机 | false 关机
#define ICBricks_GetDevicePowerStatus() (Power_Conversion)

/**主控器 led 控制
 * 参数:
 * false 禁止 电量变化
 * true 允许 电量变化
 *
 * 注意:
 * 如果允许电量变化,该函数会默认颜色并且刷新LED
 */
void SetPowerControlLED(bool com);

/* led 控制
* 参数：
* lednum 要控制led的数量
* r,g, 颜色
*
*/
void SetLEDColor(uint lednum,uint8_t r,uint8_t g,uint8_t b);

/* led 控制 刷新
 */
void RefreshLEDColor();

/*等待直到开 or 关机
 * 函数作用： 等待ICBricks 直到开 or 关机
 * 参数:state  true 开机 or false 关机
 *
 */
void ICBricks_WaitForPowerState(bool state);


// FreeRTOS 任务 - 作用为当BLE蓝牙连接和断开控制LED
void ICBricks_BLEControlLEDTask(void *pvParameters);
// 检测按键按下播放声音
void playSoundWhenKeyPressedTask(void *pvParameters);



/*恢复主控器,细节:
 *  1.扫描iic，并且关闭所有端口上输出设备
 *  2.恢复主控器电量对led的控制权
 *  3.系统程序（各种变量复位）*
 */
void RestoreMainControllerState();
#endif

#endif