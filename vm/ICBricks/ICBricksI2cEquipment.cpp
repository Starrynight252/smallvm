#if defined(ICBRICKS)
#include "ICBricks.h"
#include "ICBricksI2cEquipment.h"

#include <Wire.h>

// 手势库文件
#include <SparkFun_APDS9960.h>
// 距离
#include <VL53L0X.h>
#include "I2Cdev.h"
// 陀螺仪
#include <MPU6050.h>
// LIS3DH 陀螺仪
#include <Adafruit_LIS3DH.h>
#include <Adafruit_Sensor.h>

// APDS9960 的全局变量
SparkFun_APDS9960 apds = SparkFun_APDS9960();
// VL53L0X结构类型变量
VL53L0X sensor;

// 陀螺仪
MPU6050 MPU6050_accelgyro;
Adafruit_LIS3DH LIS3DH_accelgyro;
// 数据
sensors_event_t LIS3DH_event;

// 编码器 数据
class Encoder_Data
{
public:
    int16_t rotate;    // 旋转
    bool Kstate;       // 按键状态 *true 按下   false松开
    bool keys, netKey; // 保存按键原始值

    void GetData();          // 获取数据
    void ButtonProcessing(); // 按钮处理
    // 全部清空
    void Empty();
};
Encoder_Data Encoder_Control[8];

/*      iic 设备地址
 *@ 常量 IN_DEVICE_NUM  保存输入设备数量
 *@ 常量 OUT_DEVICE_NUM 保存输出设备数量
 *  开始元素保存输入设备
 */
IIC_DeviceAddress iic_DeviceAddress[] = {
    // ENCODER_Addre, VL53l0_Addre, MPU6050_Addre, APDS9960_Addre, PCF8591T_Addre, line_WALKING_Addre, LIS3DH_Addre, // IN
    IICLED_Addre, DRV8830_Addre, SERVO_MOTOR // out
};

// /传感器注册表
IIC_DeviceAddress SensorRegistry[ICBRICKS_TOTAL_PORTS];

// 打开端口
bool IIC_TCA9548APWR_Open(uint8_t port2)
{

    if (port2 > 7) // 0-7
        return true;

    Wire.beginTransmission(0X70); // address
    delay(1);
    Wire.write(1 << port2);
    delay(1);
    bool t = Wire.endTransmission();
    delay(1);

    return t;
}

//扫描
bool scanIICAddress(uint8_t port, uint8_t address)
{
    bool addre = false;
    IIC_TCA9548APWR_Open(port);
    // 开始检测
    Wire.beginTransmission(address);
    delay(1);
    addre = Wire.endTransmission();

    return !addre;
}

#include "interp.h"

// 设备扫描并且初始化
void IIC_DeviceUpdates(uint8_t port, IIC_DeviceAddress iicaddress)
{
    bool addre = false;

    // Serial.printf("port%d_init|%#x|\n", port, iicaddress);

    // Wire.end();
    //  delay(100);
    // ForceOne_EnableI2CCommunication();
    // delay(100);

    IIC_TCA9548APWR_Open(port);
    delay(1);
    // 开始检测
    Wire.beginTransmission(iicaddress);
    delay(1);
    addre = Wire.endTransmission();

    // /* 未检测到并且为陀螺仪，进行切换陀螺仪
    //  * 为了适配mpu6050和LIS3DH，可让其无感使用
    //  */
    if (addre && (iicaddress == MPU6050_Addre || iicaddress == LIS3DH_Addre))
    {
        iicaddress = ((iicaddress == MPU6050_Addre) ? LIS3DH_Addre : MPU6050_Addre);
        IIC_TCA9548APWR_Open(port);
        // 开始检测
        Wire.beginTransmission(iicaddress);
        delay(1);
        addre = Wire.endTransmission();

        if (addre)
        {
            iicaddress = MPU6050_Addre;
        }
    }

    // 扫描到,执行判断
    if (!addre && (iicaddress != SensorRegistry[port])) // 地址是否一样
    {
        // Serial.printf("init|%#x|----ok\n", iicaddress);
        //   保存扫描地址 更新
        SensorRegistry[port] = iicaddress;
        IIC_TCA9548APWR_Open(port);

        // 初始化
        IICDevice_Initialize(SensorRegistry[port], port);
    }
    else if (addre && (iicaddress == SensorRegistry[port]))
    {
        // Serial.printf("init|%#x|----no\n", iicaddress);
        IIC_TCA9548APWR_Open(port);
        // 开始检测
        Wire.beginTransmission(SensorRegistry[port]);
        delay(1);
        addre = Wire.endTransmission();

        if (addre) // 如果无法访问
        {
            SensorRegistry[port] = IIC_NULL;
            // Serial.printf("|%#x|init-de\n", iicaddress);
        }
        // Serial.printf("init|%#x|----no_exit\n", iicaddress);
    }

    delay(5);
}

// 获取编码器
int16_t GetEncoderData(uint8_t port, uint8_t mods)
{
    if (SensorRegistry[port] != ENCODER_Addre)
        return 0;

    Encoder_Data edata, edataS;
    int16_t temporary = 0;

    IIC_TCA9548APWR_Open(port);
    edata.GetData();
    edataS.GetData();

    if (mods == 0) // 获取旋转方向
    {
#if 0
        // 数据判断
        bool hasChanged = false;
        uint8_t samplesNum = 6;
        int8_t sampledata[6];
        uint tempms =500;

        // 清空，并且初始化为0
        for (uint8_t i = 0; i < samplesNum; i++)
        {
            sampledata[i] = 0;
        }

        //sampledata[0] = edata.rotate;


               Serial.printf(">>i%dedata.rotate%d\t", 0, edata.rotate);

       Serial.printf(">>i%dedata.rotate%d\t", 1, edataS.rotate);
    //     // 采集数据
    //     for (uint8_t i = 0; i < samplesNum; i++)
    //     {
    //         sampledata[i] = 0;//edata.rotate;
    //         Serial.printf(">>i%dedata.rotate%d\t", i, edata.rotate);
    //         (delay(tempms));
    //         edata.GetData();
    //          (delay(tempms));
    //     }

        Serial.printf("\n");
        // 检查数据
        for (uint8_t i = 0; i < samplesNum; i++)
        {
            // 只要有变化
            if (sampledata[i] != Encoder_Control[port].rotate) // sampledata[samplesNum - i - 1])
            {
                Serial.printf("i{%d}sampledata%d and Encoder_Control%d\n", i, sampledata[i], Encoder_Control[port].rotate);
                edata.rotate = sampledata[i];
                hasChanged = true;
                break;
            }
        }

        if (hasChanged)
        {

            Serial.printf("ok\n");
            if (edata.rotate > 0)
            {
                temporary = 2; // R

                if (edata.rotate < Encoder_Control[port].rotate) // 递减
                    temporary = 1;                               // L
            }
            else if (edata.rotate < 0)
            {
                temporary = 1; // L

                if (edata.rotate > Encoder_Control[port].rotate) // 递增
                    temporary = 2;                               // R
            }
        }
#else

        // 按键无变化
        if (Encoder_Control[port].keys == (edata.keys))
        {
            // 数轴判断方法
            if (!edata.rotate || (Encoder_Control[port].rotate == edata.rotate))
            {
                temporary = 0;
            }
            else
            {
                if (edata.rotate > 0)
                {
                    temporary = 2; // R

                    if (edata.rotate < Encoder_Control[port].rotate) // 递减
                        temporary = 1;                               // L
                }
                else if (edata.rotate < 0)
                {
                    temporary = 1; // L

                    if (edata.rotate > Encoder_Control[port].rotate) // 递增
                        temporary = 2;                               // R
                }
            }
        }
        else
        {
            // 按键有变化
            Encoder_Control[port].keys = edata.keys;
        }
#endif
    }
    else if (mods == 1) // 获取按键状态
    {

        temporary = !edata.keys;

        Encoder_Control[port].keys = edata.keys;
    }
    else // 获取数值
    {
        temporary = edata.rotate;
    }
    // 更新数据
    Encoder_Control[port].rotate = edata.rotate;

    return temporary;
}

// 编码器全部清空
void Encoder_Data::Empty()
{
    rotate = 0;     // 旋转
    Kstate = false; // 状态 使用ENCODER_KEY_ToBool可以判断

    keys = netKey = 0; // 按键
}

void Encoder_Data ::GetData() // 获取数据
{
    Wire.requestFrom(ENCODER_Addre, 3);

    delay(1);
    if (Wire.available())
    {
        // 二个int(8)拼接为(16),直接返回
        this->rotate = (int16_t)EIGHT_BITS_COMBINED_WITH_SIXTEEN_BITS(Wire.read(), Wire.read());

        this->keys = Wire.read();
    }
}

// 编码器按钮处理
void Encoder_Data ::ButtonProcessing()
{
    // 采集变化
    if (keys == 1 && netKey == 0)
    {
        // LOOK
        if (Kstate == false)
            Kstate = true;
        else
            Kstate = false;
    }
    // 保存变化
    netKey = keys;
}

// IIC_APDS9960_获取手势
int IIC_APDS9960_GetGesture(uint8_t port)
{
    if (SensorRegistry[port] != APDS9960_Addre)
        return 0;

    IIC_TCA9548APWR_Open(port);
    int getsure = 0;
    if (apds.isGestureAvailable())
    {

        getsure = apds.readGesture();

        // 转换手势到目标值
        switch (getsure)
        {
        case DIR_UP:
            getsure = 2;
            break;
        case DIR_DOWN:
            getsure = 1;
            break;
        case DIR_LEFT:
            getsure = 3;
            break;
        case DIR_RIGHT:
            getsure = 4;
            break;
        }
    }

    return getsure;
}

// VL53L0 返回测量距离值-毫米
uint16_t IIC_VL53L0_Continue(uint8_t port)
{
    if (SensorRegistry[port] != VL53l0_Addre)
        return 0;

    IIC_TCA9548APWR_Open(port);
    uint16_t num = 0;
    // ensor.startContinuous(1);
    //  获取测量值-毫米  readRangeSingleMillimeters
    num = sensor.readRangeSingleMillimeters();

    // 检测是否超时
    if (sensor.timeoutOccurred() || num == 65535)
    {
#if BASICS_DEBUG_UNIFY
        Serial.printf("\nVL53L0X!!!!!!!!");
#endif
        // IIC_TCA9548APWR_Open(port);
        // // 超时 重新初始化
        // IICDevice_Initialize(VL53l0_Addre,port);
        // delay(5);
        num = 0;
    }

    // num = num / 10;
    return num;
}

// 陀螺仪初始化
void MPU6050_GyroInitialization()
{
    // 默认初始化：
    // 陀螺仪 getTemperature();获取温度 setTempFIFOEnabled(1);
    // MPU6050_accelgyro.initialize();
    // MPU6050_accelgyro.testConnection();
    // MPU6050_accelgyro.setXAccelOffset(0); // Set your accelerometer offset for axis X
    // MPU6050_accelgyro.setYAccelOffset(0); // Set your accelerometer offset for axis Y
    // MPU6050_accelgyro.setZAccelOffset(0); // Set your accelerometer offset for axis Z
    // MPU6050_accelgyro.setXGyroOffset(0);  // Set your gyro offset for axis X
    // MPU6050_accelgyro.setYGyroOffset(0);  // Set your gyro offset for axis Y
    // MPU6050_accelgyro.setZGyroOffset(0);  // Set your gyro offset for axis Z

    // 自定义初始化：
    MPU6050_accelgyro.setClockSource((MPU6050_IMU::MPU6050_CLOCK_PLL_XGYRO));     // 设置时钟源 -内部
    MPU6050_accelgyro.setFullScaleGyroRange((MPU6050_IMU::MPU6050_GYRO_FS_2000)); // 陀螺仪的满量程范围
    MPU6050_accelgyro.setFullScaleAccelRange((MPU6050_IMU::MPU6050_ACCEL_FS_2));  // 设置加速范围

#if MPU6050_SENSOR_ENABLED
    MPU6050_accelgyro.setTempSensorEnabled(true); // 温度传感器使能
    MPU6050_accelgyro.getTempSensorEnabled();     // 温度传感器启用
#endif

    MPU6050_accelgyro.setSleepEnabled(false); // 设置睡眠 false关闭
}

// 获取陀螺仪轴数据
void Get_MPU6050GyroData(Gyroscope_NUM *gyroscope)
{
    if (gyroscope != NULL)
    { // 结构体，推荐结构指针.动态分配内存

        MPU6050_accelgyro.getMotion6(&gyroscope->ax, &gyroscope->ay, &gyroscope->az, &gyroscope->gx, &gyroscope->gy, &gyroscope->gz);
        // Serial.printf("MPU6050_X(%d)Y(%d)Z(%d)|gX(%d),Y(%d)Z(%d)\n", gyroscope->ax, gyroscope->ay, gyroscope->az, gyroscope->gx, gyroscope->gy, gyroscope->gz);
#if MPU6050_SENSOR_ENABLED
        gyroscope.temp = Get_GyroTemperature_Data(); // 获取温度数据
#endif
    }
}

// LIS3DH_Addre陀螺仪初始化
void LIS3DH_GyroInitialization()
{
    LIS3DH_accelgyro = Adafruit_LIS3DH();
    // delay(5);
    LIS3DH_accelgyro.begin(LIS3DH_Addre);
    LIS3DH_accelgyro.setDataRate(LIS3DH_DATARATE_50_HZ);
}

// 获取陀螺仪轴数据
void Get_LIS3DHGyroData(Gyroscope_NUM *gyroscope)
{

    int range = 25;

    if (gyroscope != NULL)
    {
        LIS3DH_accelgyro.read(); // 立即获取X Y和Z数据

        // 映射为MPU6050方向
        gyroscope->ax = ((LIS3DH_accelgyro.x > 0) ? (0 - LIS3DH_accelgyro.x) : abs(LIS3DH_accelgyro.x));
        gyroscope->ay = ((LIS3DH_accelgyro.y > 0) ? (0 - LIS3DH_accelgyro.y) : abs(LIS3DH_accelgyro.y));
        gyroscope->az = LIS3DH_accelgyro.z;
        gyroscope->gx = LIS3DH_accelgyro.x_g;
        gyroscope->gy = LIS3DH_accelgyro.y_g;
        gyroscope->gz = LIS3DH_accelgyro.z_g;
    }
}

Gyroscope_NUM Gyroscope;

/* 获取获取陀螺仪轴
 * 作用描述：
 * 根据参数port自动判断，port上面为mpu6050还是LIS3DH
 * 根据参数axis判断，获取对应轴。
 * 返回 陀螺仪轴
 */
int Get_GyroscopeAxis(uint8_t port, uint8_t axis)
{
    Gyroscope_NUM Sendgyroscope;

    int gaxis = -1;

    IIC_TCA9548APWR_Open(port);
    if (SensorRegistry[port] == MPU6050_Addre)
    {
        Get_MPU6050GyroData(&Gyroscope);
    }
    else if (SensorRegistry[port] == LIS3DH_Addre)
    {
        Get_LIS3DHGyroData(&Gyroscope);
    }
    else
    {
        return 0;
    }

    if (axis == 1)
    {

        // 范围限制 x
        if (Gyroscope.ax < 0) // 负数
        {
            Sendgyroscope.ax = (Gyroscope.ax >= MPU6050_XR_RANGE_MIN && Gyroscope.ax <= 0) ? 0 : abs(Gyroscope.ax);
            Sendgyroscope.ax = map(Sendgyroscope.ax, 0, MPU6050_XR_RANGE_MAX, 0, SCOPE_USE_GYROSCOPE);

            Sendgyroscope.ax = NEGATION(Sendgyroscope.ax);
        }
        else // 正数
        {
            Sendgyroscope.ax = (Gyroscope.ax <= MPU6050_XL_RANGE_MIN && Gyroscope.ax >= 1) ? 0 : Gyroscope.ax;
            Sendgyroscope.ax = map(Sendgyroscope.ax, 0, MPU6050_XL_RANGE_MAX, 0, SCOPE_USE_GYROSCOPE);
        }

        gaxis = Sendgyroscope.ax;
    }
    else if (axis == 2)
    {

        // 范围限制 y
        if (Gyroscope.ay < 0) // 负数
        {
            // 负数
            Sendgyroscope.ay = (Gyroscope.ay >= MPU6050_YR_RANGE_MIN && Gyroscope.ay <= 0) ? 0 : abs(Gyroscope.ay);
            Sendgyroscope.ay = map(Sendgyroscope.ay, 0, MPU6050_YL_RANGE_MAX, 0, SCOPE_USE_GYROSCOPE);

            Sendgyroscope.ay = NEGATION(Sendgyroscope.ay);
        }
        else // 正数
        {
            Sendgyroscope.ay = (Gyroscope.ay <= MPU6050_YL_RANGE_MIN && Gyroscope.ay >= 1) ? 0 : Gyroscope.ay;
            Sendgyroscope.ay = map(Sendgyroscope.ay, 0, MPU6050_YL_RANGE_MAX, 0, SCOPE_USE_GYROSCOPE);
        }
        gaxis = Sendgyroscope.ay;
    }

    return gaxis;
}

/* 获取陀螺仪方向
 * 作用描述：
 * 根据参数port自动判断，port上面为mpu6050还是LIS3DH
 * 根据参数direction判断，获取对应轴。
 * 返回 bool
 */
bool Get_GyroscopeOrientation(uint8_t port, uint8_t direction)
{
    bool valueAxis = false;
    IIC_TCA9548APWR_Open(port);

    int x = Get_GyroscopeAxis(port, 1);
    int y = Get_GyroscopeAxis(port, 2);

    if (direction == 0) // 是否水平
    {
        // 误差 +- 10
        if ((y >= -10 && y <= 10) && (x >= -10 && x <= 10))
            valueAxis = true;
    }
    else if (direction == 1) // 左
    {
        // 下                     //上
        if (x < -GYROSCOPE_ORIENTATION_VALUE)
        {
            valueAxis = !(y < -GYROSCOPE_ORIENTATION_VALUE || y > GYROSCOPE_ORIENTATION_VALUE);
            // valueAxis = (y < -6200) ? false : (y > 6050) ? false
        }
        else
            valueAxis = false;
    }
    else if (direction == 2) // 右
    {
        if ((x > GYROSCOPE_ORIENTATION_VALUE)) // 下
        {
            // 上
            // valueAxis = (y < -6200) ? false : (y > 6050) ? false
            valueAxis = !(y < -GYROSCOPE_ORIENTATION_VALUE || y > GYROSCOPE_ORIENTATION_VALUE);
        }
        else
            valueAxis = false;
    }
    else if (direction == 3) // 上
    {
        // 旧的范围：valueAxis = (y > 6010);

        if ((y > GYROSCOPE_ORIENTATION_VALUE)) // 左                右
        {
            valueAxis = !(x < -GYROSCOPE_ORIENTATION_VALUE || x > GYROSCOPE_ORIENTATION_VALUE);
        }
        else
            valueAxis = false;
    }
    else if (direction == 4) // 下
    {
        /*旧的范围： valueAxis = (y < -6150);*/

        if ((y < -GYROSCOPE_ORIENTATION_VALUE)) // 左                                  右
        {
            // valueAxis = (x < -9170) ? false : (x > 9000) ? false
            valueAxis = !(x < -GYROSCOPE_ORIENTATION_VALUE || x > GYROSCOPE_ORIENTATION_VALUE);
        }
        else
            valueAxis = false;
    }

    return valueAxis;
}

// iic设备初始化
bool IICDevice_Initialize(IIC_DeviceAddress iicaddress, uint8_t port)
{
    IIC_TCA9548APWR_Open(port);
    bool condition = true;

    switch (iicaddress)
    {
    case APDS9960_Addre:
        // APDS初始化
        apds.init();
        // 启用手势传感器
        apds.enableGestureSensor(true);
        break;

    case MPU6050_Addre:
        // 陀螺仪初始化
        MPU6050_GyroInitialization();
        break;

    case LIS3DH_Addre:
        // LIS3DH_Addre陀螺仪初始化
        LIS3DH_GyroInitialization();
        break;

    case VL53l0_Addre:
        sensor.setTimeout(100);
        sensor.init();
        // sensor.startContinuous(10);
        break;

    case ENCODER_Addre:
        Encoder_Control[port].Empty();
        break;

    default:
        condition = false;
        break;
    }

    return condition;
}

/*out**************************OUT*******************out******************out******************out*/

// 伺服电机-根据协议来控制
void IIC_SERVO_MOTOR_Control(char reg, int speed, int times)
{
    Wire.beginTransmission(SERVO_MOTOR);

    // 限制寄存器在范围
    if ((reg < 0x07 || reg == 0x70) || reg == 0x79)
        Wire.write(reg); // 寄存器

    if (speed > 0)
    {
        // 限制范围
        speed = (speed > SERVO_MOTOR_MAX) ? SERVO_MOTOR_MAX : speed;
    }
    else if (speed < 0 && speed != 0)
    {
        speed = abs(speed); // 取反
        speed = (speed > SERVO_MOTOR_MAX) ? SERVO_MOTOR_MAX : speed;
        speed = NEGATION(speed); // 取反
    }

    Wire.write(speed); // 速度

    if (times == 0)
    {
        Wire.write(0X00); // 时间
        Wire.write(0x00); // 时间
    }
    else
    {
        // 拆位
        Wire.write(SIXTEEN_SPLIT_HIGH_EIGHT_BIT(times)); // 时间
        Wire.write(SIXTEEN_SPLIT_LOW_EIGHT_BIT(times));  // 时间
    }

    Wire.endTransmission();
}

// 伺服电机-根据协议来读入 长度4 size(0~3)
void IIC_SERVO_MOTOR_Read(long *read)
{
    if (read != NULL || read != nullptr)
    {
        // 准备读入-请求字节
        Wire.requestFrom(SERVO_MOTOR, 6);

        if (Wire.available())
        {
            *(read + 0) = Wire.read(); // 速度
            // 位置
            *(read + 1) = EIGHT_BITS_COMBINED_WITH_SIXTEEN_BITS(Wire.read(), Wire.read()); // 位置
            // 功率
            *(read + 2) = EIGHT_BITS_COMBINED_WITH_SIXTEEN_BITS(Wire.read(), Wire.read()); // 功率
            // 标志位
            *(read + 3) = Wire.read(); // 标志位
        }

        if ((read[1] >> 15) & 1)
        {
            read[1] ^= (1 << 15);

            read[1] = 0 - (32768 - read[1]);
        }
    }

    return;
}

void IIC_LED(uint8_t R, uint8_t G, uint8_t B)
{

    Wire.beginTransmission(IICLED_Addre);

    Wire.write(!((R | R | G) & 1)); // 打开LED

    Wire.write(0x00); // 亮度

    Wire.write(0X01); // 控制颜色

    Wire.write(R); // R
    Wire.write(G); // G
    Wire.write(B); // B

    Wire.endTransmission();
}

// 关闭端口上输出设备
void ShutdownOutputDevices(uint8_t port)
{
    // 扫描
    for (int e = 0; e < 3; e++)
    {
        IIC_TCA9548APWR_Open(port);

        // 开始检测
        Wire.beginTransmission(iic_DeviceAddress[e]);
        delay(1);
        // 判断检测
        bool sta = Wire.endTransmission();

        if (!sta)
        {
            IIC_TCA9548APWR_Open(port);

            delay(1);
            if (iic_DeviceAddress[e] == DRV8830_Addre || iic_DeviceAddress[e] == SERVO_MOTOR) // 电机
            {
                if (iic_DeviceAddress[e] == SERVO_MOTOR)
                {
                    IIC_SERVO_MOTOR_Control(0, 0, 0);
                }
                else
                {
                    // 停止转动
                    Wire.beginTransmission(DRV8830_Addre);
                    Wire.write(0X00); // 打开控制
                    Wire.write(0X01); // 清除控制
                    Wire.write(0x80); // 清除错误
                    delay(1);
                    Wire.endTransmission();
                }

                break;
            }
            else if (iic_DeviceAddress[e] == IICLED_Addre) // LED
            {
                // 解决led 问题 2024/9/1 有效果
                Wire.beginTransmission(IICLED_Addre);
                Wire.write(0X00); // 打开LED
                Wire.write(0x00); // 亮度
                Wire.write(0X01); // 控制颜色
                Wire.write(0x00); // R
                Wire.write(0x00); // G
                Wire.write(0x00); // B
                delay(1);
                Wire.endTransmission();

                break;
            }
        }
    }
}
#endif