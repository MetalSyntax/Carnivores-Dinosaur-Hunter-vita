#ifndef SOLOADER_AUDIO_H
#define SOLOADER_AUDIO_H

#ifdef __cplusplus
extern "C" {
#endif

void audio_init(void);
void audio_term(void);
void audio_pause(void);
void audio_resume(void);

#ifdef __cplusplus
}
#endif

#endif // SOLOADER_AUDIO_H
