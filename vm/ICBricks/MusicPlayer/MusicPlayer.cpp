#if defined(ICBRICKS)

#include "MusicPlayer.h"
#include "playTone.h"

#include <cmath>
#include <vector>
#include <esp32-hal-dac.h>
// 使用文件系统
// #include "SPIFFS.h"

#include "esp_timer.h"

Music_Playback_Driver ::Music_Playback_Driver(const void *data, uint32_t len, uint8_t volume, uint32_t srate, uint8_t pin)
{
  musicData = NULL;

  dacPin = pin;
  musicData = (uint8_t *)data;
  musicLen = len;
  bePlaying = false;
  musicTransmit = false;
  setVolume(volume);
  sampleRate = srate;
  // ledcSetup(0, 0, 13);
  //  ledcAttachPin(dacPin, 0);

  dacWrite(dacPin, 0);
}

// 音量
void Music_Playback_Driver::setVolume(uint8_t volume)
{
  musicVolume = 11 - volume;
}

/*更改采样率 默认为 22050
 *参数: srate
 *可以为空,空为22050
 */
void Music_Playback_Driver ::SampleRate(uint32_t srate)
{
  sampleRate = srate;
}

/*获取采样率
 */
uint32_t Music_Playback_Driver ::Get_SampleRate()
{
  return sampleRate;
}

// 重新加载
void Music_Playback_Driver ::anewBegin(const void *data, uint32_t len, uint32_t srate)
{
  musicData = (uint8_t *)data;
  musicLen = len;
  bePlaying = false;
  musicTransmit = false;
  sampleRate = srate;
}

// 重新播放
void Music_Playback_Driver ::againPlay()
{
  musicTransmit = false;
}

// 禁止播放
void Music_Playback_Driver ::forbidPlay()
{
  musicTransmit = true;
}

// 播放音乐
void Music_Playback_Driver ::Play_Music(bool stat)
{
#if !INTERRUPT_ROUTINE
  if (stat)
  {
    // strlen
    uint32_t SAMPLE_DELAY_US = (uint32_t)(1000000 / sampleRate);
    uint64_t count = 0;

    while (count < musicLen)
    {
      // dacWrite(dacPin, musicData[count] /1);
      dacWrite(dacPin, musicData[count] / musicVolume);
      ets_delay_us(SAMPLE_DELAY_US);
      // delayMicroseconds(SAMPLE_DELAY_US);
      ++count;

      // if(musicTransmit) break;
    }

    /*for(int t=music_data[length-1]/_volume; t>=0; t--)
    for(int t=musicData[musicLen-1] / musicVolume; t >= 0; t--)
    {
      dacWrite(dacPin, t);
      ets_delay_us(10);
      //if(musicTransmit) break;
    }*/

    dacWrite(dacPin, 0);
  }
#endif
}

// 播放音乐
void Music_Playback_Driver ::Play_Music()
{
#if !INTERRUPT_ROUTINE
  if (!musicTransmit && !bePlaying)
  {
    bePlaying = true;
    // strlen
    uint32_t SAMPLE_DELAY_US = (uint32_t)(1000000 / sampleRate);
    uint64_t count = 0;

    while (count < musicLen)
    {
      // dacWrite(dacPin, musicData[count] /1);
      dacWrite(dacPin, musicData[count] / musicVolume);
      // ets_delay_us(SAMPLE_DELAY_US);
      delayMicroseconds(SAMPLE_DELAY_US);
      ++count;
    }
    bePlaying = false;

    dacWrite(dacPin, 0);
    musicTransmit = true;
    // ledcAttachPin(dacPin, 0);
  }

#endif
}

// 当前音乐播放是否到结尾
bool Music_Playback_Driver ::Play_End()
{
  return musicTransmit;
}

// /正弦波查找表
uint8_t sineLookupTable[] = {
    // 0x80, 0x8f, 0x9f, 0xae, 0xbd, 0xca, 0xd7, 0xe2,
    // 0xeb, 0xf3, 0xf9, 0xfd, 0xff, 0xff, 0xfd, 0xf9,
    // 0xf3, 0xeb, 0xe2, 0xd7, 0xca, 0xbd, 0xae, 0x9f,
    // 0x8f, 0x80, 0x70, 0x60, 0x51, 0x42, 0x35, 0x28,
    // 0x1d, 0x14, 0x0c, 0x06, 0x02, 0x00, 0x00, 0x02,
    // 0x06, 0x0c, 0x14, 0x1d, 0x28, 0x35, 0x42, 0x51,
    // 0x60, 0x70};

    // uint8_t sineLookupTable[] = {
    // 128, 156, 183, 207, 227, 242, 252, 255,
    // 252, 242, 227, 207, 183, 156, 128, 99,
    // 72, 48, 28, 13, 3, 0, 3, 13,
    // 28, 48, 72, 99};

// 减小
 0x80, 0x9f, 0xbd, 0xd7, 0xeb, 0xf9, 0xff, 0xfd,
 0xf3, 0xe2, 0xca, 0xae, 0x8f, 0x70, 0x51, 0x35,
 0x1d, 0x0c, 0x02, 0x00, 0x06, 0x14, 0x28, 0x42,
 0x60};

const float sampleRate = 8000; // 采样率
float musicVolume = 1.000;     // 音量调整

void ToneVolume(uint8_t vo)
{
  if (vo > 10)
  {
    musicVolume = 1.000;
  }
  else
  {
    musicVolume = vo / 10.0;
  }
}

void playTone(int frequency, int duration)
{
  if (!frequency)
    return;
  int period = sampleRate / frequency;
  uint64_t count = 0;
  uint64_t maxCount = (uint64_t)(frequency * duration / 1000) * sizeof(sineLookupTable);

  while (count < maxCount)
  {
    dacWrite(25, sineLookupTable[count % sizeof(sineLookupTable)] * musicVolume);
    delayMicroseconds(period);
    ++count;
  }

  // // 添加淡出效果
  // for (int fade = musicVolume; fade > 0; fade -= 1) // 逐渐减少音量
  // {
  //   dacWrite(25, sineLookupTable[(count + (fade % sizeof(sineLookupTable))) % sizeof(sineLookupTable)] * fade);
  //   delayMicroseconds(period);
  // }

  delayMicroseconds(period); // 等待
  dacWrite(25, 0);
}

void playTone(int frequency)
{
  if (!frequency)
    return;
  int period = sampleRate / frequency; // 计算周期
  int tableSize = sizeof(sineLookupTable);
  uint64_t count = 0;
  uint64_t maxCount = (uint64_t)sampleRate * tableSize / frequency; // 计算样本点数量

  while (count < maxCount)
  {
    dacWrite(25, sineLookupTable[count % tableSize] * musicVolume); // 播放样本
    delayMicroseconds(period);                                      // 等待下一个样本点
    ++count;                                                        // 更新计数器
  }
  delayMicroseconds(period); // 等待
  dacWrite(25, 0);           // 停止输出
}

TaskHandle_t playNoteTaskHandle = NULL; // 播放任务句柄

int Frequency;
int Duration;

// 音符播放任务
void playNoteTask(void *parameter)
{
  if (!Frequency)
  {
    // 播放结束，删除任务
    vTaskDelete(NULL);
  }

  int period = sampleRate / Frequency;
  uint64_t count = 0;
  uint64_t maxCount = (uint64_t)(Frequency * Duration / 1000) * sizeof(sineLookupTable);

  while (count < maxCount)
  {
    dacWrite(25, sineLookupTable[count % sizeof(sineLookupTable)] * musicVolume);
    delayMicroseconds(period);
    dacWrite(25, sineLookupTable[count % sizeof(sineLookupTable)] * musicVolume);
    ++count;
  }

  // // 添加淡出效果
  // for (int fade = musicVolume; fade > 0; fade -= 1) // 逐渐减少音量
  // {
  //   dacWrite(25, sineLookupTable[(count + (fade % sizeof(sineLookupTable))) % sizeof(sineLookupTable)] * fade);
  //   delayMicroseconds(period);
  // }
  delayMicroseconds(period); // 等待
  dacWrite(25, 0);
  // 播放结束，删除任务
  vTaskDelete(NULL);
  playNoteTaskHandle = NULL;
}

// 设置播放音符频率
void playNoteFrequency(int frequency, int durationMs)
{
  Frequency = frequency;
  Duration = durationMs;

  // 如果任务已经在运行，先停止并删除
  if (playNoteTaskHandle != NULL)
  {
    vTaskDelete(playNoteTaskHandle);
    playNoteTaskHandle = NULL;
  }

  // 重新创建并启动播放任务
  // xTaskCreate(playNoteTask, "PlayNoteTask", 2048, (void *)durationMs, 0, &playNoteTaskHandle);

  //xTaskCreatePinnedToCore(playNoteTask, "PlayNoteTask", 2048, (void *)durationMs, configMAX_PRIORITIES - 1, &playNoteTaskHandle, 1);
}

#endif