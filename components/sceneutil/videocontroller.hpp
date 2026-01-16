#ifndef OPENMW_COMPONENTS_SCENEUTIL_VIDEOCONTROLLER_H
#define OPENMW_COMPONENTS_SCENEUTIL_VIDEOCONTROLLER_H

#include <components/sceneutil/statesetupdater.hpp>

#include <osg/Texture2D>

#include <iosfwd>
#include <memory>
#include <string>

namespace Video
{
    class VideoPlayer;
    class MovieAudioFactory;
}

namespace MWSound
{
    class MovieAudioFactory;
}

namespace SceneUtil
{
    /// @brief Controller that plays video on a 3D object's texture slot.
    /// @par Updates an osg::Texture2D each frame with decoded video frames from FFmpeg.
    /// @note Must be attached to a node as an UpdateCallback or CullCallback.
    class VideoController : public StateSetUpdater
    {
    public:
        /// @brief Construct a VideoController.
        /// @param texSlot The texture unit slot to apply the video texture to (default: 0)
        /// @param loop Whether to loop the video when it ends (default: false)
        VideoController(int texSlot = 0, bool loop = false);

        VideoController(const VideoController& copy, const osg::CopyOp& copyop);

        virtual ~VideoController();

        META_Object(SceneUtil, VideoController)

        /// @brief Start playing a video from a stream.
        /// @param stream The input stream containing video data. Takes ownership.
        /// @param name A name for the video (used for logging)
        /// @param audioFactory Optional audio factory for sound playback. Takes ownership.
        void playVideo(std::unique_ptr<std::istream> stream, const std::string& name,
                       Video::MovieAudioFactory* audioFactory = nullptr);

        /// @brief Stop the currently playing video.
        void stop();

        /// @brief Pause video playback.
        void pause();

        /// @brief Resume video playback.
        void resume();

        /// @brief Check if video is currently paused.
        bool isPaused() const;

        /// @brief Check if a video is currently playing.
        bool isPlaying() const;

        /// @brief Get current playback position in seconds.
        double getCurrentTime() const;

        /// @brief Get total video duration in seconds.
        double getDuration() const;

        /// @brief Seek to a specific time in the video.
        /// @param time Time in seconds
        void seek(double time);

        /// @brief Set whether the video should loop.
        void setLoop(bool loop) { mLoop = loop; }

        /// @brief Check if looping is enabled.
        bool getLoop() const { return mLoop; }

        /// @brief Get the texture slot this controller writes to.
        int getTexSlot() const { return mTexSlot; }

        /// @brief Set audio volume (0.0 to 1.0)
        void setVolume(float volume);

        /// @brief Get current audio volume
        float getVolume() const;

        /// Apply the current video frame to the stateset.
        /// Called automatically each frame when attached as a callback.
        void apply(osg::StateSet* stateset, osg::NodeVisitor* nv) override;

    protected:
        void setDefaults(osg::StateSet* stateset) override;

    private:
        std::unique_ptr<Video::VideoPlayer> mVideoPlayer;
        int mTexSlot;
        bool mLoop;
        bool mPlaying;
        std::string mVideoPath; // Store path for looping
        MWSound::MovieAudioFactory* mAudioFactory = nullptr; // Non-owning pointer for volume control
        float mVolume = 1.0f;
    };

} // namespace SceneUtil

#endif // OPENMW_COMPONENTS_SCENEUTIL_VIDEOCONTROLLER_H
