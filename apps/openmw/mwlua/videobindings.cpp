#include "videobindings.hpp"

#include <components/debug/debuglog.hpp>
#include <components/files/istreamptr.hpp>
#include <components/lua/luastate.hpp>
#include <components/misc/strings/algorithm.hpp>
#include <components/resource/resourcesystem.hpp>
#include <components/sceneutil/videocontroller.hpp>
#include <components/vfs/manager.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/world.hpp"

#include "../mwrender/animation.hpp"

#include "../mwsound/movieaudiofactory.hpp"

#include "context.hpp"
#include "luamanagerimp.hpp"
#include "object.hpp"

namespace MWLua
{
    namespace
    {
        const MWWorld::Ptr& getMutablePtrOrThrow(const Object& object)
        {
            const MWWorld::Ptr& ptr = object.ptr();
            if (!ptr.getRefData().isEnabled())
                throw std::runtime_error("Can't use a disabled object");
            return ptr;
        }

        MWRender::Animation* getMutableAnimationOrThrow(const Object& object)
        {
            const MWWorld::Ptr& ptr = getMutablePtrOrThrow(object);
            auto world = MWBase::Environment::get().getWorld();
            MWRender::Animation* anim = world->getAnimation(ptr);
            if (!anim)
                throw std::runtime_error("Object has no animation");
            return anim;
        }

        // Custom visitor that finds ANY node by name (not just Groups)
        class FindAnyNodeByNameVisitor : public osg::NodeVisitor
        {
        public:
            std::string mNameToFind;
            osg::Node* mFoundNode = nullptr;

            FindAnyNodeByNameVisitor(const std::string& name)
                : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN)
                , mNameToFind(name)
            {
            }

            void apply(osg::Node& node) override
            {
                if (!mFoundNode && Misc::StringUtils::ciEqual(node.getName(), mNameToFind))
                {
                    mFoundNode = &node;
                    return;
                }
                traverse(node);
            }
        };

        // Find a node by name in an object's scene graph (returns any node type)
        osg::Node* findNodeByName(MWRender::Animation* anim, const std::string& nodeName)
        {
            osg::Group* objectRoot = anim->getObjectRoot();
            if (!objectRoot)
            {
                Log(Debug::Warning) << "VideoController: objectRoot is null";
                return nullptr;
            }

            FindAnyNodeByNameVisitor visitor(nodeName);
            objectRoot->accept(visitor);
            return visitor.mFoundNode;
        }

        // Simple handle for controlling video playback
        // Stored in Lua as userdata
        struct VideoHandle
        {
            osg::ref_ptr<SceneUtil::VideoController> controller;
            std::string nodeName;
            Object object;

            bool isValid() const { return controller != nullptr; }

            // Get the node, looking it up fresh each time
            osg::Node* getNode() const
            {
                if (!controller)
                    return nullptr;
                try
                {
                    MWRender::Animation* anim = getMutableAnimationOrThrow(object);
                    return findNodeByName(anim, nodeName);
                }
                catch (...)
                {
                    return nullptr;
                }
            }
        };
    }

    sol::table initVideoPackage(const Context& context)
    {
        auto view = context.sol();

        // Register VideoHandle usertype
        auto handleType = view.new_usertype<VideoHandle>("VideoHandle",
            sol::no_constructor);

        handleType["isPlaying"] = [](const VideoHandle& handle) -> bool {
            if (!handle.isValid())
                return false;
            return handle.controller->isPlaying();
        };

        handleType["isPaused"] = [](const VideoHandle& handle) -> bool {
            if (!handle.isValid())
                return false;
            return handle.controller->isPaused();
        };

        handleType["pause"] = [context](VideoHandle& handle) {
            if (!handle.isValid())
                return;
            context.mLuaManager->addAction(
                [controller = handle.controller]() {
                    controller->pause();
                },
                "videoPauseAction");
        };

        handleType["resume"] = [context](VideoHandle& handle) {
            if (!handle.isValid())
                return;
            context.mLuaManager->addAction(
                [controller = handle.controller]() {
                    controller->resume();
                },
                "videoResumeAction");
        };

        handleType["stop"] = [context](VideoHandle& handle) {
            if (!handle.isValid())
                return;
            context.mLuaManager->addAction(
                [controller = handle.controller, nodeName = handle.nodeName,
                 object = handle.object]() {
                    controller->stop();
                    // Remove the controller from the node
                    try
                    {
                        MWRender::Animation* anim = getMutableAnimationOrThrow(object);
                        osg::Node* node = findNodeByName(anim, nodeName);
                        if (node)
                            node->removeUpdateCallback(controller);
                    }
                    catch (std::exception& e)
                    {
                        Log(Debug::Warning) << "VideoController: Could not remove callback: " << e.what();
                    }
                },
                "videoStopAction");
            // Invalidate the handle
            handle.controller = nullptr;
        };

        handleType["getCurrentTime"] = [](const VideoHandle& handle) -> double {
            if (!handle.isValid())
                return 0.0;
            return handle.controller->getCurrentTime();
        };

        handleType["getDuration"] = [](const VideoHandle& handle) -> double {
            if (!handle.isValid())
                return 0.0;
            return handle.controller->getDuration();
        };

        handleType["seek"] = [context](VideoHandle& handle, double time) {
            if (!handle.isValid())
                return;
            context.mLuaManager->addAction(
                [controller = handle.controller, time]() {
                    controller->seek(time);
                },
                "videoSeekAction");
        };

        handleType["setLoop"] = [](VideoHandle& handle, bool loop) {
            if (!handle.isValid())
                return;
            handle.controller->setLoop(loop);
        };

        handleType["setVolume"] = [](VideoHandle& handle, float volume) {
            if (!handle.isValid())
                return;
            handle.controller->setVolume(volume);
        };

        handleType["getVolume"] = [](const VideoHandle& handle) -> float {
            if (!handle.isValid())
                return 0.0f;
            return handle.controller->getVolume();
        };

        handleType["getObjectPosition"] = [](const VideoHandle& handle) -> sol::optional<osg::Vec3f> {
            if (!handle.isValid())
                return sol::nullopt;
            try
            {
                const MWWorld::Ptr& ptr = handle.object.ptr();
                if (ptr.isEmpty())
                    return sol::nullopt;
                return ptr.getRefData().getPosition().asVec3();
            }
            catch (...)
            {
                return sol::nullopt;
            }
        };

        // Create the video API table
        sol::table api(view, sol::create);

        // Main function to play video on a node
        // Usage: local handle = video.playOnNode(object, "video_screen", "video/myvideo.mp4", { loop = true })
        api["playOnNode"] = [context](const SelfObject& object, std::string nodeName,
                                       std::string videoPath, sol::optional<sol::table> options) -> VideoHandle {
            bool loop = false;
            int texSlot = 0;
            bool withAudio = true;
            float volume = 1.0f;

            if (options)
            {
                loop = options->get_or("loop", false);
                texSlot = options->get_or("textureSlot", 0);
                withAudio = options->get_or("audio", true);
                volume = options->get_or("volume", 1.0f);
            }

            // Create the controller
            auto controller = osg::ref_ptr<SceneUtil::VideoController>(
                new SceneUtil::VideoController(texSlot, loop));

            // Set initial volume
            if (volume != 1.0f)
                controller->setVolume(volume);

            // Set up the handle with object and node name for later reference
            VideoHandle handle;
            handle.controller = controller;
            handle.nodeName = nodeName;
            handle.object = Object(object);

            // Queue the actual video loading and node attachment on the main thread
            context.mLuaManager->addAction(
                [object = Object(object), nodeName, videoPath, controller, withAudio]() {
                    try
                    {
                        MWRender::Animation* anim = getMutableAnimationOrThrow(object);
                        osg::Node* node = findNodeByName(anim, nodeName);

                        if (!node)
                        {
                            Log(Debug::Error) << "VideoController: Node '" << nodeName << "' not found";
                            return;
                        }

                        // Get VFS and open the video file
                        const VFS::Manager* vfs = MWBase::Environment::get().getResourceSystem()->getVFS();

                        Files::IStreamPtr videoStream;
                        try
                        {
                            videoStream = vfs->get(videoPath);
                        }
                        catch (std::exception& e)
                        {
                            Log(Debug::Error) << "VideoController: Failed to open video '" << videoPath << "': " << e.what();
                            return;
                        }

                        // Set up audio factory if requested
                        Video::MovieAudioFactory* audioFactory = nullptr;
                        if (withAudio)
                            audioFactory = new MWSound::MovieAudioFactory();

                        // Start playing the video
                        controller->playVideo(std::move(videoStream), videoPath, audioFactory);

                        // Attach the controller to the node
                        node->addUpdateCallback(controller);

                        Log(Debug::Info) << "VideoController: Playing '" << videoPath << "' on node '" << nodeName << "'";
                    }
                    catch (std::exception& e)
                    {
                        Log(Debug::Error) << "VideoController: Error: " << e.what();
                    }
                },
                "videoPlayOnNodeAction");

            return handle;
        };

        return LuaUtil::makeReadOnly(api);
    }

} // namespace MWLua
