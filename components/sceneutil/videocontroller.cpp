#include "videocontroller.hpp"

#include <algorithm>
#include <istream>

#include <osg/TexMat>
#include <osg-ffmpeg-videoplayer/videoplayer.hpp>

#include <apps/openmw/mwsound/movieaudiofactory.hpp>

#include <components/debug/debuglog.hpp>

namespace SceneUtil
{

    VideoController::VideoController(int texSlot, bool loop)
        : mVideoPlayer(std::make_unique<Video::VideoPlayer>())
        , mTexSlot(texSlot)
        , mLoop(loop)
        , mPlaying(false)
    {
    }

    VideoController::VideoController(const VideoController& copy, const osg::CopyOp& copyop)
        : StateSetUpdater(copy, copyop)
        , mVideoPlayer(std::make_unique<Video::VideoPlayer>())
        , mTexSlot(copy.mTexSlot)
        , mLoop(copy.mLoop)
        , mPlaying(false)
        , mVideoPath(copy.mVideoPath)
    {
        // Note: We create a new VideoPlayer, we don't copy the video state.
        // If the original was playing, the copy starts stopped.
    }

    VideoController::~VideoController() = default;

    void VideoController::playVideo(std::unique_ptr<std::istream> stream, const std::string& name,
                                     Video::MovieAudioFactory* audioFactory)
    {
        // Store name for logging
        mVideoPath = name;

        if (audioFactory)
        {
            // Store pointer for volume control (cast to our specific type)
            mAudioFactory = dynamic_cast<MWSound::MovieAudioFactory*>(audioFactory);
            mVideoPlayer->setAudioFactory(audioFactory);

            // Apply any pre-set volume
            if (mAudioFactory && mVolume != 1.0f)
                mAudioFactory->setVolume(mVolume);
        }

        mVideoPlayer->playVideo(std::move(stream), name);
        mPlaying = true;
    }

    void VideoController::stop()
    {
        Log(Debug::Info) << "VideoController::stop() - start";

        // Clear audio track reference before closing to avoid dangling pointer
        if (mAudioFactory)
        {
            Log(Debug::Info) << "VideoController::stop() - clearing audio track";
            mAudioFactory->clearAudioTrack();
        }
        mAudioFactory = nullptr;

        Log(Debug::Info) << "VideoController::stop() - closing video player";
        mVideoPlayer->close();
        Log(Debug::Info) << "VideoController::stop() - video player closed";
        mPlaying = false;
        Log(Debug::Info) << "VideoController::stop() - done";
    }

    void VideoController::pause()
    {
        mVideoPlayer->pause();
    }

    void VideoController::resume()
    {
        mVideoPlayer->play();
    }

    bool VideoController::isPaused() const
    {
        return mVideoPlayer->isPaused();
    }

    bool VideoController::isPlaying() const
    {
        return mPlaying;
    }

    double VideoController::getCurrentTime() const
    {
        return mVideoPlayer->getCurrentTime();
    }

    double VideoController::getDuration() const
    {
        return mVideoPlayer->getDuration();
    }

    void VideoController::seek(double time)
    {
        mVideoPlayer->seek(time);
    }

    void VideoController::setDefaults(osg::StateSet* stateset)
    {
        // Nothing to set up initially - texture will be applied when video starts
    }

    void VideoController::apply(osg::StateSet* stateset, osg::NodeVisitor* nv)
    {
        if (!mPlaying)
        {
            // Only remove texture attributes once, not every frame
            return;
        }

        // Update the video player to decode the next frame
        bool stillPlaying = mVideoPlayer->update();

        if (!stillPlaying)
        {
            if (mLoop)
            {
                // Seek back to beginning for looping
                mVideoPlayer->seek(0);
                mVideoPlayer->play();
                // Don't access texture this frame - wait for seek to complete
                return;
            }
            else
            {
                // Clear audio track reference before it gets destroyed
                if (mAudioFactory)
                    mAudioFactory->clearAudioTrack();
                mAudioFactory = nullptr;
                mPlaying = false;
                return;
            }
        }

        // Get the current video texture and apply it to the stateset
        osg::ref_ptr<osg::Texture2D> texture = mVideoPlayer->getVideoTexture();
        if (texture && texture->getImage())
        {
            stateset->setTextureAttributeAndModes(mTexSlot, texture, osg::StateAttribute::ON);

            // FFmpeg frames are top-left origin, OpenGL expects bottom-left
            // Always apply texture matrix to flip the V coordinate
            osg::ref_ptr<osg::TexMat> texMat = new osg::TexMat;
            texMat->setMatrix(osg::Matrix::scale(1, -1, 1) * osg::Matrix::translate(0, 1, 0));
            stateset->setTextureAttributeAndModes(mTexSlot, texMat, osg::StateAttribute::ON);
        }
    }

    void VideoController::setVolume(float volume)
    {
        mVolume = std::max(0.0f, std::min(1.0f, volume));
        if (mAudioFactory)
            mAudioFactory->setVolume(mVolume);
    }

    float VideoController::getVolume() const
    {
        return mVolume;
    }

} // namespace SceneUtil
