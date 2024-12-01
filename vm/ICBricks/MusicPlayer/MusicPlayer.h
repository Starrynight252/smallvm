#ifndef _MUSIC_PLAYER_H_
#define _MUSIC_PLAYER_H_

#if defined(ICBRICKS)
#include <Arduino.h>


#define INTERRUPT_ROUTINE 0

// 音乐播放模式
typedef uint8_t PlayMode;

#define SAMPLE_RATE 22000 // 25650  //22600

#define MUSIC_LEN 8 // Number of notes

// 音乐结构-音乐播放器驱动
class Music_Playback_Driver
{
public:
    // 构造函数， 默认DAC PIN 25  使用SPIFFS文件系统
    // Music_Playback_Driver(const char* opentext,uint8_t volume = 8,uint32_t srate= SAMPLE_RATE,uint8_t pin =25);
    // 构造函数， 默认DAC PIN 25  使用PROGMEM数组
    Music_Playback_Driver(const void *data, uint32_t len, uint8_t volume = 8, uint32_t srate = SAMPLE_RATE, uint8_t pin = 25);

    // 析构函数
    ~Music_Playback_Driver() {}

    // 初始化
    void begin();
    /*-----------音乐时间类-----------------*/
    // 播放音乐
    void Play_Music();
    // 播放音乐
    void Play_Music(bool s);
    // 当前音乐播放是否到结尾
    bool Play_End();
    // 音量
    void setVolume(uint8_t volume);
    /*更改采样率 默认为 22050
     *参数: srate
     *可以为空,空为22050
     */
    void SampleRate(uint32_t srate = SAMPLE_RATE);
    // 获取采样率
    uint32_t Get_SampleRate();

    // 重新加载
    void anewBegin(const void *data, uint32_t len, uint32_t srate = SAMPLE_RATE);
    // 重新播放
    void againPlay();
    // 禁止播放
    void forbidPlay();

    /*针对用户隐藏的成员- 用户不可以直接使用*/
private:
    // 播放状态
    bool musicTransmit = false;
    // 正在播放
    bool bePlaying;

    // 采样率
    uint32_t sampleRate;
    // 播放大小
    uint32_t musicLen;

    // 播放的文件
    const uint8_t *musicData;

    uint8_t musicVolume;
    // dac脚位
    uint8_t dacPin;
};

#endif
#endif