#ifndef MUSIC_H
#define MUSIC_H

#include <filesystem>


namespace music {


bool Start(const std::filesystem::path &directory);


void Update();


void Stop();

void ToggleMute();
void ChangeVolume(float delta); 

} 

#endif
