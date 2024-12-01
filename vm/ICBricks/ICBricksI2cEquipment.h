#ifndef _ICBricksI2cEquipment_H_
#define _ICBricksI2cEquipment_H_

#if defined(ICBRICKS)
// IIC 端口总数量
#define ICBRICKS_TOTAL_PORTS 8

#include <stdint.h>

// IIC 设备地址
typedef enum
{
    IIC_NULL = 0x00,

    // IN
    ENCODER_Addre = 0x36,      // 编码器
    VL53l0_Addre = 0x29,       // 距离
    MPU6050_Addre = 0x68,      // 陀螺仪
    APDS9960_Addre = 0x39,     // 手势
    PCF8591T_Addre = 0X48,     // 声音
    line_WALKING_Addre = 0x02, // 巡线传感器
    LIS3DH_Addre = 0x18,       // new陀螺仪
    // OUT
    IICLED_Addre = 0x40,  // LED
    DRV8830_Addre = 0X67, // 电机
    SERVO_MOTOR = 0x50,   // 伺服电机

} IIC_DeviceAddress;
// 陀螺仪数据
typedef struct
{
    int16_t ax, ay, az; // 加速计
    int16_t gx, gy, gz;
#if MPU6050_SENSOR_ENABLED
    double temp; // 温度
#endif
} Gyroscope_NUM;



// 陀螺仪限制范围min y
#define MPU6050_YR_RANGE_MIN -1700
#define MPU6050_YL_RANGE_MIN 1000
// 陀螺仪限制范围max y
#define MPU6050_YR_RANGE_MAX 16000
#define MPU6050_YL_RANGE_MAX 15500

// 陀螺仪限制范围min x
#define MPU6050_XR_RANGE_MIN -500
#define MPU6050_XL_RANGE_MIN 2068

// 陀螺仪限制范围max x
#define MPU6050_XR_RANGE_MAX 15500
#define MPU6050_XL_RANGE_MAX 16700

// /陀螺仪使用范围
#define SCOPE_USE_GYROSCOPE 90

// 陀螺仪方向值
#define GYROSCOPE_ORIENTATION_VALUE 20
// 电机
#define SERVO_MOTOR_MAX 50
// 取反
#define NEGATION(num) ((num) * (-1.0))

/********************IIC操作*************** */
// 打开端口
bool IIC_TCA9548APWR_Open(uint8_t port);
//扫描
bool scanIICAddress(uint8_t port, uint8_t address);
// iic设备初始化
bool IICDevice_Initialize(IIC_DeviceAddress iicaddress,uint8_t port);
// 设备扫描并且初始化
void IIC_DeviceUpdates(uint8_t port,IIC_DeviceAddress iicaddress);
// 关闭端口上输出设备
void ShutdownOutputDevices(uint8_t port);


// 获取编码器
int16_t GetEncoderData(uint8_t port, uint8_t mods);
/********************手势******************* */
// IIC_APDS9960_获取手势
int IIC_APDS9960_GetGesture(uint8_t port);


/********************测距****************** */
// VL53L0 返回测量距离值-毫米
uint16_t IIC_VL53L0_Continue(uint8_t port);

/********************陀螺仪***************** */
// 获取陀螺仪轴数据
// 获取陀螺仪轴数据
void Get_MPU6050GyroData(Gyroscope_NUM *gyroscope);

// 获取陀螺仪轴数据
void Get_LIS3DHGyroData(Gyroscope_NUM *gyroscope);

/* 获取获取陀螺仪轴
 * 作用描述：
 * 根据参数port自动判断，port上面为mpu6050还是LIS3DH
 * 根据参数axis判断，获取对应轴。
 * 返回 陀螺仪轴
 */
int Get_GyroscopeAxis(uint8_t port, uint8_t axis);
/* 获取陀螺仪方向
 * 作用描述：
 * 根据参数port自动判断，port上面为mpu6050还是LIS3DH
 * 根据参数direction判断，获取对应轴。
 * 返回 bool
 */
bool Get_GyroscopeOrientation(uint8_t port, uint8_t direction);


//out************************out************out
// 伺服电机-根据协议来控制
void IIC_SERVO_MOTOR_Control(char reg, int speed, int times);
void IIC_LED(uint8_t R, uint8_t G, uint8_t B);
// 伺服电机-根据协议来读入 长度4 size(0~3)
void IIC_SERVO_MOTOR_Read(long *read);
#endif
#endif