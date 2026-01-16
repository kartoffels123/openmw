#ifndef OPENMW_MWSOUND_MOVIEAUDIOFACTORY_H
#define OPENMW_MWSOUND_MOVIEAUDIOFACTORY_H

#include <osg-ffmpeg-videoplayer/audiofactory.hpp>

namespace MWSound
{
    class Stream;

    class MovieAudioFactory : public Video::MovieAudioFactory
    {
    public:
        std::unique_ptr<Video::MovieAudioDecoder> createDecoder(Video::VideoState* videoState) override;

        /// @brief Set the volume of the audio track (0.0 to 1.0)
        void setVolume(float volume);

        /// @brief Get the current volume of the audio track
        float getVolume() const { return mVolume; }

        /// @brief Check if audio is currently playing
        bool hasAudio() const { return mAudioTrack != nullptr; }

        /// @brief Clear the audio track reference (call before video closes)
        void clearAudioTrack() { mAudioTrack = nullptr; }

    private:
        Stream* mAudioTrack = nullptr;
        float mVolume = 1.0f;
    };

}

#endif
