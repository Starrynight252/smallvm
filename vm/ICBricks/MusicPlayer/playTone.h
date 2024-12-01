#ifndef _PLAY_TONE_H_
#define _PLAY_TONE_H_
#if defined(ICBRICKS)

// 基础音符频率与音名的对应
// const int note_DO = 262;   // C
// const int note_RE = 294;   // D
// const int note_MI = 330;   // E
// const int note_FA = 349;   // F
// const int note_SOL = 392;  // G
// const int note_LA = 440;   // A
// const int note_SI = 494;   // B
// const int note_DO2 = 523;  // C2

//FreeRTOS 音符播放任务 -- 由MusicPlayer.cpp 自动管理，请勿使用。
void playNoteTask(void *parameter);

// 生成正弦波表
void generateSineTable();
// 启动音符播放;
void playNoteFrequency(int frequency, int durationMs);

//设置音量大小
void ToneVolume(uint8_t vo);
/*播放音符
* frequency  音符
* duration   播放时间(#可选)
*/
void playTone(int frequency, int duration);
/*播放音符
* frequency  音符
*/
void playTone(int frequency);

#endif
#endif