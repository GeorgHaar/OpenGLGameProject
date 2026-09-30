#include "music.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {
constexpr int READ_AHEAD_SAMPLES = 32768; 
constexpr float DEFAULT_VOLUME = 0.8f;

constexpr float TARGET_PEAK = 0.89f;
constexpr float MAX_MAKEUP_GAIN = 10.0f;
constexpr float MIN_MAKEUP_GAIN = 0.5f;
constexpr double SCAN_SECONDS = 30.0; 

struct Player {
    juce::AudioDeviceManager deviceManager;
    juce::AudioFormatManager formats;
    juce::AudioSourcePlayer output;
    juce::AudioTransportSource transport;
    juce::TimeSliceThread readAhead{"Musik"};
    std::unique_ptr<juce::AudioFormatReaderSource> reader;
    std::filesystem::path directory;
    std::vector<std::string> playlist;
    std::size_t index = 0;
    float volume = DEFAULT_VOLUME;
    float makeupGain = 1.0f;
    bool muted = false;
    bool playing = false;

    ~Player() {
        deviceManager.removeAudioCallback(&output);
        output.setSource(nullptr);
        transport.setSource(nullptr);
        reader.reset();
        readAhead.stopThread(2000);
        deviceManager.closeAudioDevice();
    }
};


std::optional<juce::ScopedJuceInitialiser_GUI> juceRuntime;
std::unique_ptr<Player> player;
int frameCounter = 0;

std::string Percent(float volume) {
    return std::to_string(int(std::lround(volume * 100.0f))) + " %";
}

std::string Decibel(float gain) {
    const float db = 20.0f * std::log10(std::max(gain, 1e-6f));
    return (db >= 0.0f ? "+" : "") + std::to_string(int(std::lround(db))) + " dB";
}

void ApplyGain() {
    player->transport.setGain(player->muted ? 0.0f : player->volume * player->makeupGain);
}

bool OpenDevice(Player &p) {
    const juce::String problem = p.deviceManager.initialiseWithDefaultDevices(0, 2);
    if (problem.isNotEmpty()) {
        std::cout << "Musik: Kein Audiogeraet: " << problem << std::endl;
        return false;
    }

    if (juce::AudioIODevice *device = p.deviceManager.getCurrentAudioDevice()) {
        double rate = 0.0;
        for (double candidate : device->getAvailableSampleRates())
            if (candidate == 48000.0 || (candidate == 44100.0 && rate == 0.0)) rate = candidate;
        if (rate > 0.0 && rate != device->getCurrentSampleRate()) {
            juce::AudioDeviceManager::AudioDeviceSetup setup = p.deviceManager.getAudioDeviceSetup();
            setup.sampleRate = rate;
            const juce::String rateProblem = p.deviceManager.setAudioDeviceSetup(setup, true);
            if (rateProblem.isNotEmpty())
                std::cout << "Musik: Abtastrate " << rate << " Hz nicht moeglich: " << rateProblem << std::endl;
        }
    }
    juce::AudioIODevice *device = p.deviceManager.getCurrentAudioDevice();
    if (!device) {
        std::cout << "Musik: Das Audiogeraet liess sich nicht oeffnen." << std::endl;
        return false;
    }
    p.readAhead.startThread();
    p.output.setSource(&p.transport);
    p.deviceManager.addAudioCallback(&p.output);
    std::cout << "Musik: Audiogeraet \"" << device->getName() << "\" geoeffnet, "
              << device->getCurrentSampleRate() << " Hz, Puffer " << device->getCurrentBufferSizeSamples()
              << " Samples" << std::endl;
    return true;
}


float MeasurePeak(juce::AudioFormatReader &reader) {
    const int channels = std::min(2, int(reader.numChannels));
    if (channels <= 0) return 0.0f;
    const juce::int64 samples = std::min(reader.lengthInSamples, juce::int64(reader.sampleRate * SCAN_SECONDS));
    juce::Range<float> levels[2];
    reader.readMaxLevels(0, samples, levels, channels);
    float peak = 0.0f;
    for (int channel = 0; channel < channels; ++channel)
        peak = std::max({peak, std::abs(levels[channel].getStart()), std::abs(levels[channel].getEnd())});
    return peak;
}

bool Load(Player &p, const std::string &track, bool loop) {
    const juce::File file(juce::String((p.directory / track).string()));
    std::unique_ptr<juce::AudioFormatReader> formatReader(p.formats.createReaderFor(file));
    if (!formatReader || formatReader->lengthInSamples <= 0) {
        std::cout << "Musik: Die Datei konnte nicht gelesen werden: " << track << std::endl;
        return false;
    }
   
    p.transport.stop();
    p.transport.setSource(nullptr);
    p.reader.reset();
    const float peak = MeasurePeak(*formatReader);
    p.makeupGain = peak > 0.0f ? std::clamp(TARGET_PEAK / peak, MIN_MAKEUP_GAIN, MAX_MAKEUP_GAIN) : 1.0f;
    const double sampleRate = formatReader->sampleRate;
    p.reader = std::make_unique<juce::AudioFormatReaderSource>(formatReader.release(), true);
    p.reader->setLooping(loop);
    p.transport.setSource(p.reader.get(), READ_AHEAD_SAMPLES, &p.readAhead, sampleRate);
    ApplyGain();
    p.transport.setPosition(0.0);
    p.transport.start();
    p.playing = true;
    std::cout << "Musik: " << track << (loop ? " (Endlosschleife)" : "") << ", Pegel " << Decibel(p.makeupGain)
              << std::endl;
    return true;
}
}

namespace music {

bool Start(const std::filesystem::path &directory) {
    juceRuntime.emplace();
    player = std::make_unique<Player>();
    player->directory = directory;
    player->formats.registerBasicFormats();
    if (!OpenDevice(*player)) {
        Stop();
        return false;
    }

    std::error_code error;
    std::vector<std::string> names;
    for (const auto &entry : std::filesystem::directory_iterator(directory, error))
        if (entry.is_regular_file(error)) names.push_back(entry.path().filename().string());
    std::sort(names.begin(), names.end());
    for (const std::string &name : names) {
        const juce::File file(juce::String((directory / name).string()));
        if (player->formats.findFormatForFileExtension(juce::String(std::filesystem::path(name).extension().string())) == nullptr)
            continue;
        std::unique_ptr<juce::AudioFormatReader> reader(player->formats.createReaderFor(file));
        if (reader && reader->lengthInSamples > 0) player->playlist.push_back(name);
        else std::cout << "Musik: Die Datei wird uebersprungen (nicht lesbar): " << name << std::endl;
    }
    if (player->playlist.empty()) {
        std::cout << "Musik: Keine Musik in " << directory << " gefunden (WAV, MP3, OGG, FLAC oder AIFF)." << std::endl;
        return false;
    }
    return Load(*player, player->playlist.front(), player->playlist.size() == 1);
}

void Update() {
    if (!player) return;
    if (++frameCounter % 60 == 0) juce::MessageManager::getInstance()->runDispatchLoopUntil(0);
    if (player->playing && player->playlist.size() > 1 && player->transport.hasStreamFinished()) {
        player->index = (player->index + 1) % player->playlist.size();
        Load(*player, player->playlist[player->index], false);
    }
}

void Stop() {
    player.reset();
    juceRuntime.reset();
}

void ToggleMute() {
    if (!player) return;
    player->muted = !player->muted;
    ApplyGain();
    std::cout << "Musik: " << (player->muted ? "stumm" : "an, Lautstaerke " + Percent(player->volume)) << std::endl;
}

void ChangeVolume(float delta) {
    if (!player) return;
    player->volume = std::clamp(player->volume + delta, 0.0f, 1.0f);
    ApplyGain();
    std::cout << "Musik: Lautstaerke " << Percent(player->volume) << (player->muted ? " (stumm)" : "") << std::endl;
}

} 
