/* 该文件创建于2024/10/11
 *  支持 ICBricks功能
 *
 */
#include "ICBricks.h"

#if defined(ICBRICKS)
// LED
#include "FastLED.h"

// i2c
#include "ICBricksI2cEquipment.h"
// dac声音
#include "MusicPlayer/MusicPlayer.h"
#include "MusicPlayer/MusicSource.h"
#include "MusicPlayer/playTone.h"
// 获取ble状态
#include "interp.h"

#define BUTTON1_MASK (1 << 0)
#define BUTTON2_MASK (1 << 1)
#define BUTTON3_MASK (1 << 2)
#define BUTTON4_MASK (1 << 3)

// LED 数量的数组-用vm\ICBricks\ICBricks.cpp于保存LED的颜色
CRGB leds_Colour[5] = {CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Black, CRGB::Black};
// 开机音播放器 11110
Music_Playback_Driver StartupTone(startup_Sound, (sizeof(startup_Sound) * sizeof(uint8_t)), 10);

// 电源开关状态 true 启动|false 停止
bool Power_Conversion = false;
/*----开关机/电源----*/
// 开关机计时  之前
ulong Power_PreviousTime = 0;

// 队列句柄 按键状态
volatile uint8_t ButtonState = 0x00;
// 队列句柄 按键播放声音状态
volatile uint8_t ButtonSoundPlayed = 0xff;
// 全局LED亮度的最大值
#define MAX_LED_BRIGHTNESS 35

// 全部初始化
void ICBricks_Initializ()
{

    Power_PreviousTime = 0;
    Power_Conversion = false;

    pinMode(ICBRICKS_KEY_0, INPUT_PULLUP);
    pinMode(ICBRICKS_KEY_1, INPUT_PULLUP);
    pinMode(ICBRICKS_KEY_2, INPUT_PULLUP);
    pinMode(ICBRICKS_KEY_3, INPUT_PULLUP);
    pinMode(ICBRICKS_KEY_4, INPUT_PULLUP);

    pinMode(ICBRICKS_CHARGE, INPUT_PULLUP);
    pinMode(ICBRICKS_VBAT_A, INPUT_PULLUP);
    pinMode(ICBRICKS_POWER_GIO_IN, INPUT_PULLUP); // POWER_GIO_IN 设置为输入
    pinMode(ICBRICKS_POWER_GIO_OUT, OUTPUT);

    // 初始化       <LED类型,   GIO     ,LED颜色顺序  >  （led的数组，数量)
    FastLED.addLeds<ICBRICKS_LED_TYPE, ICBRICKS_LED_GIO, ICBRICKS_LED_COLOR_ORDER>(leds_Colour, ICBRICKS_LED_NUMS);

    FastLED.setBrightness(MAX_LED_BRIGHTNESS); // 设置LED亮度
    leds_Colour[0] = CRGB::Black;
    leds_Colour[1] = CRGB::Black;
    leds_Colour[2] = CRGB::Black;
    leds_Colour[3] = CRGB::Black;

    leds_Colour[4].r = 88;
    leds_Colour[4].g = 255;
    leds_Colour[4].b = 67;

    // leds_Colour[4] = CRGB::Black; // Blue
    FastLED.show(); // 刷新LED

    xTaskCreatePinnedToCore(
        ICBricks_SwitchMachine_Task, // 函数指针
        "Power",                     // 任务描述
        1024 * 3,                    // 堆栈大小configMINIMAL_STACK_SIZE
        NULL,                        // 参数值
        4,                           // 优先级
        NULL,                        // 句柄
        0                            // 核心 core
    );

    xTaskCreatePinnedToCore(
        playSoundWhenKeyPressedTask, // 函数指针
        "KeyPressSound",             // 任务描述
        1024 * 2,                    // 堆栈大小configMINIMAL_STACK_SIZE
        NULL,                        // 参数值
        3,                           // 优先级
        NULL,                        // 句柄
        0                            // 核心 core
    );

    xTaskCreatePinnedToCore(
        ICBricks_BLEControlLEDTask, // 函数指针
        "BLEControlLED",            // 任务描述
        1024 * 2,                   // 堆栈大小configMINIMAL_STACK_SIZE
        NULL,                       // 参数值
        2,                          // 优先级
        NULL,                       // 句柄
        0                           // 核心 core
    );
}

void ICBricks_WaitForPowerState(bool state)
{
    do
    {
        vTaskDelay(pdMS_TO_TICKS(10));
    } while (ICBricks_GetDevicePowerStatus() == state);
}

// 获取电池电量
uint8_t GetbatteryLevel()
{
    uint8_t capacity = 0x00;

    uint16_t vbat = analogRead(ICBRICKS_VBAT_A);
    // 映射
    vbat = (vbat <= BATTERY_CAPACITY_MAX) ? vbat : BATTERY_CAPACITY_MAX;

    // 限制最大范围
    capacity = (vbat - 2000) * (255 - 0) / (3300 - 2000) + 0;

    return capacity;
}

// 关机充电LED显示
void Charging_LEDdisplay()
{
    static char charging = 10;

    if (digitalRead(ICBRICKS_FULLY)) // 充满电
    {

        leds_Colour[4] = CRGB::Green;

        FastLED.show(); // 刷新LED
    }
    else // 充电中
    {
        leds_Colour[0] = CRGB::Black;
        leds_Colour[1] = CRGB::Black;
        leds_Colour[2] = CRGB::Black;
        leds_Colour[3] = CRGB::Black;
        leds_Colour[4] = CHSV(255, 255, charging);

        FastLED.show(); // 刷新LED
        delay(7);

        if (charging != 0xff)
            ++charging;
        else
        {
            while (charging > 10)
            {
                --charging;
                leds_Colour[4] = CHSV(255, 255, charging);

                FastLED.show(); // 刷新LED
                delay(7);
            }
        }
    }
}

/* gp脚本控制状态
 * 0x00 关闭
 * 0x01 触发启动
 */
volatile uint8_t GpScriptControlState = 0x00;

// 电量指示灯
volatile bool PowerIndicator_Light = true;
// 颜色锁
static bool colorLock = false;
// 电量操作
static uint8_t cycleIndex = 0;
static uint16_t Bsum = 0;
static uint16_t Bvbat = 0;
// 电池电量0~255
uint8_t BatteryLevel = 0xff;

// 电量led显示
void inline Battery_LedDisplay(TickType_t *xLastWakeTime)
{
    // 求平均
    Bvbat = analogRead(ICBRICKS_VBAT_A); // CHARGE
    // 映射
    Bvbat = (Bvbat <= BATTERY_CAPACITY_MAX) ? Bvbat : BATTERY_CAPACITY_MAX;

    Bsum += Bvbat;

    ++cycleIndex;

    if (cycleIndex >= 10)
    {
        Bsum /= 10; // 求平均
        // Bsum = (Bsum <= 200) ? 0 : Bsum;
        // 限制最大范围
        // BatteryLevel  = (Bsum *BATTERY_CAPACITY_MAX) / 255;

        BatteryLevel = (Bsum - 2500) * (255 - 0) / (BATTERY_CAPACITY_MAX - 2500) + 0;

        Bsum = Bvbat = cycleIndex = 0; // 置位为0
    }

    if (PowerIndicator_Light) // 电量指示灯开启状态
    {
        if (BatteryLevel <= HALF_CHARGE) // 200
        {
            // 如果充电则颜色锁关闭
            if (!digitalRead(ICBRICKS_TYPE_CC))
                colorLock = true;

            if (BatteryLevel > WARNING_BATTERY_LEVEL)
            {
                for (char i = 0; i < 4; i++) // Yellow -> Redb
                {
                    leds_Colour[i].r = 0xFF;
                    leds_Colour[i].g = BatteryLevel;
                    leds_Colour[i].b = 0x00;
                }
            }
            else if (BatteryLevel <= WARNING_BATTERY_LEVEL)
            {
                for (char i = 0; i < 4; i++) // Yellow -> Red
                {
                    leds_Colour[i].r = 0xFF;
                    leds_Colour[i].g = ((BatteryLevel > 0x19)) ? BatteryLevel : 0x00; // 25
                    leds_Colour[i].b = 0x00;
                }
            }

            FastLED.show(); // 刷新LED
        }
        // else if (BatteryLevel > HALF_CHARGE && !colorLock)
        // {

        //   if (!UserProgram.RunningStatus()) // 用户程序没启动
        //   {
        //     for (char i = 0; i < 4; i++) // Yellow -> Red
        //     {
        //       leds_Colour[i] = CRGB::Green;
        //       // leds_Colour[i].nscale8(10);  //设置LED亮度 ICBRICKS_VBAT_A
        //     }

        //     FastLED.show(); // 刷新LED
        //   }
        // }
    }
    if (xLastWakeTime == NULL)
    {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    else
    {
        vTaskDelayUntil(xLastWakeTime, pdMS_TO_TICKS(10));
    }
}

void SetPowerControlLED(bool com)
{
    // false 禁止 电量变化
    PowerIndicator_Light = com;

    if (Power_Conversion)
    {
        if (PowerIndicator_Light)
        {
            // 设置默认颜色

            leds_Colour[0] = CRGB::Green;
            leds_Colour[1] = CRGB::Green;
            leds_Colour[2] = CRGB::Green;
            leds_Colour[3] = CRGB::Green;
            // leds_Colour[4] = CRGB::Blue;//Blue

            FastLED.show(); // 刷新LED
        }

        Battery_LedDisplay(NULL);
    }
}

/* led 颜色控制
 * 参数：
 * lednum 要控制led的数量
 * r,g, 颜色
 */
void SetLEDColor(uint lednum, uint8_t r, uint8_t g, uint8_t b)
{
    leds_Colour[lednum].r = r;
    leds_Colour[lednum].g = g;
    leds_Colour[lednum].b = b;

    // FastLED.show(); // 刷新LED
}

/* led 控制 刷新
 */
void RefreshLEDColor()
{
    FastLED.show(); // 刷新LED
}

bool ICBricks_GetKeyPressStatus(uint8_t pin, bool sound)
{
    bool retu = false;

    if (pin == ICBRICKS_KEY_0)
    {
        retu = (ButtonState & BUTTON1_MASK);

        if (sound)
        {
            ButtonSoundPlayed |= BUTTON1_MASK;
        }
        else
        {
            ButtonSoundPlayed &= ~BUTTON1_MASK;
        }
    }

    if (pin == ICBRICKS_KEY_1)
    {
        retu = (ButtonState & BUTTON2_MASK);

        if (sound)
        {
            ButtonSoundPlayed |= BUTTON2_MASK;
        }
        else
        {
            ButtonSoundPlayed &= ~BUTTON2_MASK;
        }
    }

    if (pin == ICBRICKS_KEY_2)
    {
        retu = (ButtonState & BUTTON3_MASK);

        if (sound)
        {
            ButtonSoundPlayed |= BUTTON3_MASK;
        }
        else
        {
            ButtonSoundPlayed &= ~BUTTON3_MASK;
        }
    }

    if (pin == ICBRICKS_KEY_3)
    {
        retu = (ButtonState & BUTTON4_MASK);

        if (sound)
        {
            ButtonSoundPlayed |= BUTTON4_MASK;
        }
        else
        {
            ButtonSoundPlayed &= ~BUTTON4_MASK;
        }
    }

    return retu;
}

#define GP_SCRIPT_CONTROL_STATE_MAX 2
// FreeRTOS 任务 -设备开关机
void ICBricks_SwitchMachine_Task(void *pvParameters)
{
    bool oncemusic = false;
    ulong procedure_CurrentTime = 0; // 记录时间;
    ulong operation_StepsOutTime = 0;
    volatile uint8_t procedure_NUM = 0;
    int g_startupLedBrightness = 5;
    TickType_t xLastWakeTime = xTaskGetTickCount();

    while (true)
    {

        if (!digitalRead(ICBRICKS_POWER_GIO_IN) && !Power_PreviousTime) // 按下
        {                                                               // 第一次按下
            Power_PreviousTime = millis();                              // 记录时间
            if (Power_Conversion == false)                              // 为关机才可以开机
            {
                operation_StepsOutTime = millis();          // 记录时间
                digitalWrite(ICBRICKS_POWER_GIO_OUT, HIGH); // 输出高电平-开机
                leds_Colour[0] = CRGB::Green;
                leds_Colour[1] = CRGB::Green;
                leds_Colour[2] = CRGB::Green;
                leds_Colour[3] = CRGB::Green;
                leds_Colour[4] = CRGB::NavajoWhite;
                FastLED.setBrightness(g_startupLedBrightness);
                FastLED.show(); // 刷新LED
            }
        }
        else if (digitalRead(ICBRICKS_POWER_GIO_IN) && Power_PreviousTime) // 松手
        {
            Power_PreviousTime = 0;
            if (Power_Conversion == false) // 为关机才可以开机
            {
                digitalWrite(ICBRICKS_POWER_GIO_OUT, LOW); // 输出低电平-关机
                g_startupLedBrightness = 4;
                FastLED.setBrightness(0);
                FastLED.show(); // 刷新LED
            }

            if (Power_Conversion)
            {
                if (procedure_CurrentTime && (millis() - procedure_CurrentTime) >= 5)
                {
                    procedure_CurrentTime = 0;
                    operation_StepsOutTime = millis();
                    procedure_NUM++;
                    // Serial.printf("\nTask.OK%d\n", procedure_NUM);

                    if (procedure_NUM >= NNUMBER_GP_TRIGGERS)
                    {
                        // // 修改运行状态
                        // GpScriptControlState++;

                        // if (GpScriptControlState >= GP_SCRIPT_CONTROL_STATE_MAX)
                        //     GpScriptControlState = 0x00;

                        if (!ideConnected() && !Serial.available())
                        {
                            if (GpScriptControlState)
                            {
                                startAll();
                            }
                            GpScriptControlState = ((GpScriptControlState) ? 0 : 1);
                        }

                        procedure_NUM = 0;
                        // Serial.printf("\nTask.Done%d{Times:%d}\n", procedure_NUM, GpScriptControlState);
                    }
                }
            }
        }

        if (Power_Conversion && (digitalRead(ICBRICKS_POWER_GIO_IN) && procedure_NUM)) // 松手
        {
            if (operation_StepsOutTime && millis() - operation_StepsOutTime >= ENTER_GP_TIMEOUT)
            {
                procedure_NUM = 0;
                operation_StepsOutTime = procedure_CurrentTime = 0;
                // Serial.printf("\nTask.clse%d\n", procedure_NUM);
            }
        }

        // 判断时间
        if (!digitalRead(ICBRICKS_POWER_GIO_IN) && Power_PreviousTime) // 检测·状态
        {
            if ((millis() - Power_PreviousTime) >= TRIGGERING_GP_TIME) // 计算时间 现在-之前  >= s
            {
                procedure_CurrentTime = millis(); // 记录时间;
            }

            if (Power_Conversion == false) // 为关机才可以开机
            {
                if (millis() - operation_StepsOutTime >= 15)
                {
                    g_startupLedBrightness += 5;

                    if (g_startupLedBrightness >= MAX_LED_BRIGHTNESS)
                        g_startupLedBrightness = MAX_LED_BRIGHTNESS;

                    FastLED.setBrightness(g_startupLedBrightness);
                    FastLED.show(); // 刷新LED

                    operation_StepsOutTime = millis();
                }

                if ((millis() - Power_PreviousTime) >= ICBRICKS_POWER_INTERVAL / 4)
                {
                    if (!Power_Conversion)
                        StartupTone.Play_Music();
                }
            }

            if ((millis() - Power_PreviousTime) >= ICBRICKS_POWER_INTERVAL) // 计算时间 现在-之前  >= s
            {
                if (Power_Conversion == false) // 为关机才可以开机
                {
                    operation_StepsOutTime = 0;
                    Power_Conversion = true;
                    g_startupLedBrightness = MAX_LED_BRIGHTNESS;
                    leds_Colour[0] = CRGB::Green;
                    leds_Colour[1] = CRGB::Green;
                    leds_Colour[2] = CRGB::Green;
                    leds_Colour[3] = CRGB::Green;
                    leds_Colour[4].r = 88;
                    leds_Colour[4].g = 255;
                    leds_Colour[4].b = 67;
                    FastLED.setBrightness(MAX_LED_BRIGHTNESS);
                    digitalWrite(ICBRICKS_POWER_GIO_OUT, HIGH); // 输出高电平-开机
                    Power_PreviousTime = 0;
                    FastLED.show(); // 刷新LED

                    // StartupTone.Play_Music();
                    //  while (!StartupTone.Play_End()) ets_delay_us(1);
                }
                else
                {
                    Power_Conversion = false;

                    leds_Colour[0] = CRGB::Black;
                    leds_Colour[1] = CRGB::Black;
                    leds_Colour[2] = CRGB::Black;
                    leds_Colour[3] = CRGB::Black;
                    leds_Colour[4] = CRGB::Black; // Blue
                    FastLED.show();               // 刷新LED
                    Power_PreviousTime = 0;

                    digitalWrite(ICBRICKS_POWER_GIO_OUT, LOW); // 输出低电平-关机

                    while (digitalRead(ICBRICKS_POWER_GIO_IN) == false)
                    {
                        vTaskDelay(pdMS_TO_TICKS(10));
                    }

                    esp_restart(); // esp系统软复位
                }

                // digitalWrite(POWER_GIO_IN,HIGH);  //输出高电平

                while (digitalRead(ICBRICKS_POWER_GIO_IN) == false)
                {
                    // ets_delay_us(1);
                    vTaskDelay(pdMS_TO_TICKS(10));
                    // digitalWrite(POWER_GIO_IN,HIGH);  //输出高电平
                }
            }
        }

        if (!Power_Conversion) // 关机充电led
        {
            Charging_LEDdisplay();
        }
        else
        {
            if (ideConnected() || Serial.available())
            {
                GpScriptControlState = 1;
            }

            if (millis() % 500 >= 2)
                Battery_LedDisplay(&xLastWakeTime);

            if (!oncemusic && StartupTone.Play_End()) // 保证执行一次并且 已经播放一次
            {
                StartupTone.anewBegin(keys_Voice, (sizeof(keys_Voice) * sizeof(uint8_t))); // sizeof(keys_Voice));
                StartupTone.forbidPlay();

                oncemusic = true;
            }
            else if (oncemusic)
                StartupTone.Play_Music();
        }
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(10));
    }
}

// FreeRTOS 任务 - 作用为当BLE蓝牙连接和断开控制LED
void ICBricks_BLEControlLEDTask(void *pvParameters)
{
    // led rgb变化状态位
    bool ble_AfterClientStarts = false;
    bool ble_AfterServerStarts = false;
    uint8_t r = 88, g = 0xff, b = 67;
    ulong ledColorChangeDuration = 0;
    TickType_t xLastWakeTime = xTaskGetTickCount();

    while (true)
    {
        if (Power_Conversion)
        {
            if (BLE_connected_to_IDE && ideConnected())
            {
                if (!ledColorChangeDuration || (millis() - ledColorChangeDuration >= 15))
                {
                    ble_AfterClientStarts = true;
                    if (!ble_AfterServerStarts)
                    {
                        ble_AfterServerStarts = true;

                        // r = ((GpScriptControlState) ? 0XB0 : 88);
                        r = 88, g = 0xff, b = 67;
                    }

                    leds_Colour[4] = CHSV(r, g, b);

                    FastLED.show(); // 刷新LED

                    if (b < 0xf0)
                    {
                        r += 1;
                        b += 4;
                    }
                    else
                    {

                        ble_AfterServerStarts = false;
                    }

                    ledColorChangeDuration = millis();
                    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(2));
                }
            }
            else if (GpScriptControlState) // 程序执行中
            {

                if (!ledColorChangeDuration || (millis() - ledColorChangeDuration >= 12))
                {
                    ble_AfterClientStarts = true;
                    if (!ble_AfterServerStarts)
                    {
                        ble_AfterServerStarts = true;

                        // r = ((GpScriptControlState) ? 0XB0 : 88);
                        r = 0xf0, g = 0X88, b = 0;
                    }

                    leds_Colour[4] = CHSV(r, g, b);

                    FastLED.show(); // 刷新LED

                    if (b < 0xf0 && g > 0)
                    {
                        g -= 1;
                        b += 4;
                    }
                    else
                    {
                        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(10));
                        ble_AfterServerStarts = false;
                    }

                    ledColorChangeDuration = millis();

                    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(2));
                    // vTaskDelay(pdMS_TO_TICKS(2));
                }
            }
            else if (ble_AfterClientStarts)
            {
                ledColorChangeDuration = 0;
                ble_AfterClientStarts = false;
                leds_Colour[4].r = 88;
                leds_Colour[4].g = 255;
                leds_Colour[4].b = 67;

                FastLED.show(); // 刷新LED
            }
        }
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(10));
    }
}

// 检测按键按下播放声音
void playSoundWhenKeyPressedTask(void *pvParameters)
{
    // uint8_t buttonStateLast = 0x00;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    int playSound = true;
    uint8_t ButtonStateLast = ButtonState;
    uint8_t PlaykeySounds = 0x00;
    while (true)
    {
        if (Power_Conversion)
        {

            vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(5));
            if (!digitalRead(ICBRICKS_KEY_0))
            {
                ButtonState |= BUTTON1_MASK;
            }
            else
            {
                ButtonState &= ~BUTTON1_MASK;
                PlaykeySounds &= ~BUTTON1_MASK;
            }

            if (!digitalRead(ICBRICKS_KEY_1))
            {
                ButtonState |= BUTTON2_MASK;
            }
            else
            {
                ButtonState &= ~BUTTON2_MASK;
                PlaykeySounds &= ~BUTTON2_MASK;
            }

            if (!digitalRead(ICBRICKS_KEY_2))
            {
                ButtonState |= BUTTON3_MASK;
            }
            else
            {
                ButtonState &= ~BUTTON3_MASK;
                PlaykeySounds &= ~BUTTON3_MASK;
            }

            if (!digitalRead(ICBRICKS_KEY_3))
            {
                ButtonState |= BUTTON4_MASK;
            }
            else
            {
                ButtonState &= ~BUTTON4_MASK;
                PlaykeySounds &= ~BUTTON4_MASK;
            }

            if (ButtonState != ButtonStateLast)
            {
                if ((ButtonSoundPlayed & BUTTON1_MASK) && ((ButtonState & BUTTON1_MASK) && !(PlaykeySounds & BUTTON1_MASK)))
                {
                    PlaykeySounds |= BUTTON1_MASK;
                    // 播放声音
                    StartupTone.againPlay();
                }

                if ((ButtonSoundPlayed & BUTTON2_MASK) && ((ButtonState & BUTTON2_MASK) && !(PlaykeySounds & BUTTON2_MASK)))
                {
                    PlaykeySounds |= BUTTON2_MASK;
                    // 播放声音
                    StartupTone.againPlay();
                }

                if ((ButtonSoundPlayed & BUTTON3_MASK) && ((ButtonState & BUTTON3_MASK) && !(PlaykeySounds & BUTTON3_MASK)))
                {
                    PlaykeySounds |= BUTTON3_MASK;
                    // 播放声音
                    StartupTone.againPlay();
                }
                if ((ButtonSoundPlayed & BUTTON4_MASK) && ((ButtonState & BUTTON4_MASK) && !(PlaykeySounds & BUTTON4_MASK)))
                {
                    PlaykeySounds |= BUTTON4_MASK;
                    // 播放声音
                    StartupTone.againPlay();
                }
            }

            vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(20));

            // vTaskDelay(pdMS_TO_TICKS(10));
            ButtonStateLast = ButtonState;
        }
        else
        {
            vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(10));
        }
    }
}

/*恢复主控器,细节:
 *  1.扫描iic，并且关闭所有端口上输出设备
 *  2.恢复主控器电量对led的控制权
 *  3.系统程序（各种变量复位）*
 */
void RestoreMainControllerState()
{
    enableI2CCommunication();
    for (int port = 0; port < 10; port++)
    {
        ShutdownOutputDevices(port);
        delay(5);
    }

    // 恢复主控器电量对led的控制权
    SetPowerControlLED(true);
}

#endif