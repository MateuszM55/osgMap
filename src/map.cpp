#include <osgDB/ReadFile>
#include <osgUtil/Optimizer>
#include <osg/CoordinateSystemNode>

#include <osg/Switch>
#include <osg/Types>
#include <osgText/Text>
#include <osg/MatrixTransform>
#include <osg/ShapeDrawable>
#include <osg/ImageStream>

#include <osgViewer/Viewer>
#include <osgViewer/ViewerEventHandlers>

#include <osgGA/TrackballManipulator>
#include <osgGA/FlightManipulator>
#include <osgGA/DriveManipulator>
#include <osgGA/KeySwitchMatrixManipulator>
#include <osgGA/StateSetManipulator>
#include <osgGA/AnimationPathManipulator>
#include <osgGA/TerrainManipulator>
#include <osgGA/SphericalManipulator>

#include <osgGA/Device>


#include <iostream>
#include <future>
#include <chrono>
#include <thread>

#include "common.h"
#include "HUD.h"
#include "camera_manip.h"
#include "post_process.h"

using namespace osg;
using namespace std::chrono_literals;
float g_targetAlpha = 0.0f;
float g_currentAlpha = 0.0f;

osg::ref_ptr<osgViewer::Viewer> viewer;
osg::ref_ptr<osg::EllipsoidModel> ellipsoid;

osg::Group* create_loading_screen()
{
    std::string libName =
        osgDB::Registry::instance()->createLibraryNameForExtension("ffmpeg");
    osgDB::Registry::instance()->loadLibrary(libName);

    osg::ref_ptr<osg::Geode> geode = new osg::Geode;

    osg::StateSet* stateset = geode->getOrCreateStateSet();
    stateset->setMode(GL_LIGHTING, osg::StateAttribute::OFF);

    // clang-format off
    static const char* shaderSourceTextureVertex = R"(
        #version 420 compatibility
        out vec4 texcoord;

        void main(void)
        {
            texcoord = gl_MultiTexCoord0;
            gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
        }
        )";

    static const char* shaderSourceTexture2D = R"(
        #version 420 compatibility
        uniform sampler2D movie_texture;
        in vec4 texcoord;
        out vec4 fragColor;

        void main(void)
        {
            vec4 texture_color = texture2D(movie_texture, texcoord.st);
            fragColor = texture_color;
        }
        )";
    // clang-format on


    osg::ref_ptr<osg::Group> group = new osg::Group;
    osg::Program* program = new osg::Program;

    program->addShader(
        new osg::Shader(osg::Shader::VERTEX, shaderSourceTextureVertex));
    program->addShader(
        new osg::Shader(osg::Shader::FRAGMENT, shaderSourceTexture2D));

    stateset->addUniform(new osg::Uniform("movie_texture", 0));
    stateset->setAttribute(program);
    stateset->setMode(
        GL_BLEND, osg::StateAttribute::OFF | osg::StateAttribute::PROTECTED);

    osg::ref_ptr<osg::Image> image =
        osgDB::readRefImageFile("images/osgmap_loading.mp4");

    if (!image)
    {
        image = osgDB::readRefImageFile("images/loading.dds");
    }

    osg::Texture2D* texture = new osg::Texture2D(image);
    texture->setResizeNonPowerOfTwoHint(false);
    texture->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR);
    texture->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE);
    texture->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE);
    texture->setUnRefImageDataAfterApply(true);

    stateset->setTextureAttribute(0, texture);
    stateset->setMode(GL_BLEND, osg::StateAttribute::OFF);
    stateset->setMode(GL_CULL_FACE, osg::StateAttribute::ON);

    osg::ImageStream* imagestream =
        dynamic_cast<osg::ImageStream*>(image.get());

    if (imagestream)
    {
        imagestream->play();
    }

    if (image)
    {
        float width = image->s() * image->getPixelAspectRatio();
        float height = image->t();

        {
            geode->setCullingActive(false);

            osg::Geometry* hud = new osg::Geometry;
            osg::Vec3Array* vertices = new osg::Vec3Array;
            float depth = -0.1;
            vertices->push_back(osg::Vec3(0, height, depth));
            vertices->push_back(osg::Vec3(0, 0, depth));
            vertices->push_back(osg::Vec3(width, 0, depth));
            vertices->push_back(osg::Vec3(width, 0, depth));
            vertices->push_back(osg::Vec3(width, height, depth));
            vertices->push_back(osg::Vec3(0, height, depth));
            hud->setVertexArray(vertices);

            osg::Vec2Array* texCoords = new osg::Vec2Array;
            texCoords->push_back(osg::Vec2(0, 0));
            texCoords->push_back(osg::Vec2(0, 1));
            texCoords->push_back(osg::Vec2(1, 1));
            texCoords->push_back(osg::Vec2(1, 1));
            texCoords->push_back(osg::Vec2(1, 0));
            texCoords->push_back(osg::Vec2(0, 0));
            hud->setTexCoordArray(0, texCoords);

            hud->addPrimitiveSet(new osg::DrawArrays(GL_TRIANGLES, 0, 6));
            geode->addDrawable(hud);

            osg::MatrixTransform* modelview_abs = new osg::MatrixTransform;
            modelview_abs->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
            modelview_abs->setMatrix(osg::Matrixf::identity());
            modelview_abs->addChild(geode);

            osg::Projection* projection = new osg::Projection;
            projection->setMatrix(osg::Matrixf::ortho2D(0, width, 0, height));
            projection->addChild(modelview_abs);
            group->addChild(projection);
        }
    }

    return group.release();
}

int main(int argc, char** argv)
{
    // use an ArgumentParser object to manage the program arguments.
    osg::ArgumentParser arguments(&argc, argv);

    arguments.getApplicationUsage()->setApplicationName(
        arguments.getApplicationName());
    arguments.getApplicationUsage()->setDescription(
        arguments.getApplicationName()
        + " is the standard OpenSceneGraph example which loads and visualises "
          "3d models.");
    arguments.getApplicationUsage()->setCommandLineUsage(
        arguments.getApplicationName() + " [options] filename ...");
    arguments.getApplicationUsage()->addCommandLineOption(
        "--image <filename>", "Load an image and render it on a quad");
    arguments.getApplicationUsage()->addCommandLineOption(
        "--dem <filename>", "Load an image/DEM and render it on a HeightField");
    arguments.getApplicationUsage()->addCommandLineOption(
        "--login <url> <username> <password>",
        "Provide authentication information for http file access.");
    arguments.getApplicationUsage()->addCommandLineOption(
        "-p <filename>",
        "Play specified camera path animation file, previously saved with 'z' "
        "key.");
    arguments.getApplicationUsage()->addCommandLineOption(
        "--speed <factor>",
        "Speed factor for animation playing (1 == normal speed).");
    arguments.getApplicationUsage()->addCommandLineOption(
        "--device <device-name>", "add named device to the viewer");
    arguments.getApplicationUsage()->addCommandLineOption(
        "--stats", "print out load and compile timing stats");
    arguments.getApplicationUsage()->addCommandLineOption(
        "--max-tilt <degrees>",
        "Maximum camera tilt angle in degrees (0-90, default: 75)");

    /**
     * Even though postfx have more parameters,
     * they shouldn't really be modified by the user
     */
    arguments.getApplicationUsage()->addCommandLineOption(
        "--fxaa-search-steps <num_steps>",
        "Amount of search steps performed by FXXA (default: 8)");
    arguments.getApplicationUsage()->addCommandLineOption(
        "--fxaa-blur-close <distance_px>",
        "Close distance of the FXAA blur (default: 1px)");
    arguments.getApplicationUsage()->addCommandLineOption(
        "--fxaa-blur-far <distance_px>",
        "Far distance of the FXAA blur (default 1.5px)");

    arguments.getApplicationUsage()->addCommandLineOption(
        "--dof-max-blur <value>",
        "Maximum Depth Of Field blur intensity (default 0.03)");
    arguments.getApplicationUsage()->addCommandLineOption(
        "--dof-focus-range <value>",
        "Focus range of the Depth Of Field (default 0.986)");

    arguments.getApplicationUsage()->addCommandLineOption(
        "--bloom-threshold <value>",
        "Threshold of the Bloom effect (default 0.9)");
    arguments.getApplicationUsage()->addCommandLineOption(
        "--bloom-intensity <value>",
        "Intensity of the Bloom effect (default 2.0)");

    ellipsoid = new osg::EllipsoidModel;
    viewer = new osgViewer::Viewer(arguments);


    unsigned int helpType = 0;
    if ((helpType = arguments.readHelpType()))
    {
        arguments.getApplicationUsage()->write(std::cout, helpType);
        return 1;
    }

    // report any errors if they have occurred when parsing the program
    // arguments.
    if (arguments.errors())
    {
        arguments.writeErrorMessages(std::cout);
        return 1;
    }

    if (arguments.argc() <= 1)
    {
        arguments.getApplicationUsage()->write(
            std::cout, osg::ApplicationUsage::COMMAND_LINE_OPTION);
        return 1;
    }

    bool printStats = arguments.read("--stats");

    std::string url, username, password;
    while (arguments.read("--login", url, username, password))
    {
        osgDB::Registry::instance()
            ->getOrCreateAuthenticationMap()
            ->addAuthenticationDetails(
                url, new osgDB::AuthenticationDetails(username, password));
    }

    std::string device;
    while (arguments.read("--device", device))
    {
        osg::ref_ptr<osgGA::Device> dev =
            osgDB::readRefFile<osgGA::Device>(device);
        if (dev.valid())
        {
            viewer->addDevice(dev);
        }
    }

    std::string file_path;
    {
        if (!arguments.read("-path", file_path))
        {
            std::cout << arguments.getApplicationName()
                      << ": please provide database path (-path [path])"
                      << std::endl;
            return 0;
        }
    }

    osgMap::postfx::FXAA::Parameters fxaa_params;
    osgMap::postfx::DOF::Parameters dof_params;
    osgMap::postfx::Bloom::Parameters bloom_params;
    {
        arguments.read("--fxaa-search-steps",
            fxaa_params.number_search_steps);
        arguments.read("--fxaa-blur-close",
            fxaa_params.blur_close_distance);
        arguments.read("--fxaa-blur-far", fxaa_params.blur_far_distance);
        arguments.read("--dof-max-blur", dof_params.max_blur);
        arguments.read("--dof-focus-range", dof_params.focus_range);
        arguments.read("--bloom-threshold", bloom_params.threshold);
        arguments.read("--bloom-intensity", bloom_params.intensity);
    }

    // set up the camera manipulators.
    {
        // Read max tilt parameter from command line
        double maxTilt = 75.0; // default value
        arguments.read("--max-tilt", maxTilt);

        osg::ref_ptr<osgGA::KeySwitchMatrixManipulator> keyswitchManipulator =
            new osgGA::KeySwitchMatrixManipulator;

        // Create GoogleMapsManipulator and set max tilt
        GoogleMapsManipulator* googleMapsManip = new GoogleMapsManipulator();
        googleMapsManip->setMaxTiltDeg(maxTilt);

        keyswitchManipulator->addMatrixManipulator('1', "GoogleMaps",
                                                   googleMapsManip);
        keyswitchManipulator->addMatrixManipulator(
            '2', "Trackball", new osgGA::TrackballManipulator());
        keyswitchManipulator->addMatrixManipulator(
            '3', "Flight", new osgGA::FlightManipulator());
        keyswitchManipulator->addMatrixManipulator(
            '4', "Drive", new osgGA::DriveManipulator());
        keyswitchManipulator->addMatrixManipulator(
            '5', "Terrain", new osgGA::TerrainManipulator());
        keyswitchManipulator->addMatrixManipulator(
            '6', "Orbit", new osgGA::OrbitManipulator());
        keyswitchManipulator->addMatrixManipulator(
            '7', "FirstPerson", new osgGA::FirstPersonManipulator());
        keyswitchManipulator->addMatrixManipulator(
            '8', "Spherical", new osgGA::SphericalManipulator());

        std::string pathfile;
        double animationSpeed = 1.0;
        while (arguments.read("--speed", animationSpeed))
        {}
        char keyForAnimationPath = '8';
        while (arguments.read("-p", pathfile))
        {
            osgGA::AnimationPathManipulator* apm =
                new osgGA::AnimationPathManipulator(pathfile);
            if (apm && !apm->getAnimationPath()->empty())
            {
                apm->setTimeScale(animationSpeed);

                unsigned int num =
                    keyswitchManipulator->getNumMatrixManipulators();
                keyswitchManipulator->addMatrixManipulator(keyForAnimationPath,
                                                           "Path", apm);
                keyswitchManipulator->selectMatrixManipulator(num);
                ++keyForAnimationPath;
            }
        }

        // Wrap the key switch manipulator inside your movement tracker
        viewer->setCameraManipulator(keyswitchManipulator.get());
    }

    // add the state manipulator
    viewer->addEventHandler(new osgGA::StateSetManipulator(
        viewer->getCamera()->getOrCreateStateSet()));

    // add the thread model handler
    viewer->addEventHandler(new osgViewer::ThreadingHandler);

    // add the window size toggle handler
    viewer->addEventHandler(new osgViewer::WindowSizeHandler);

    // add the stats handler
    viewer->addEventHandler(new osgViewer::StatsHandler);

    // add the help handler
    viewer->addEventHandler(
        new osgViewer::HelpHandler(arguments.getApplicationUsage()));

    // add the record camera path handler
    viewer->addEventHandler(new osgViewer::RecordCameraPathHandler);

    // add the LOD Scale handler
    viewer->addEventHandler(new osgViewer::LODScaleHandler);

    // add the screen capture handler
    viewer->addEventHandler(new osgViewer::ScreenCaptureHandler);

    // any option left unread are converted into errors to write out later.
    arguments.reportRemainingOptionsAsUnrecognized();

    // report any errors if they have occurred when parsing the program
    // arguments.
    if (arguments.errors())
    {
        arguments.writeErrorMessages(std::cout);
        return 1;
    }

    osg::ElapsedTime elapsedTime;
    if (printStats)
    {
        double loadTime = elapsedTime.elapsedTime_m();
        std::cout << "Load time " << loadTime << "ms" << std::endl;

        viewer->getStats()->collectStats("compile", true);
    }

    /////////////////////////////////////////////////////////////////////
    //////////////////////////////////// CREATE MAP SCENE ///////////////
    /////////////////////////////////////////////////////////////////////


    osg::ref_ptr<osg::MatrixTransform> root = new osg::MatrixTransform;
    osg::ref_ptr<osg::Group> scene = new osg::Group;
    auto prepare_scene = [](osg::ref_ptr<osg::MatrixTransform>& root,
                            osg::ref_ptr<osg::Group>& scene,
                            const std::string& file_path) {
        osg::Matrixd ltw;
        osg::BoundingBox wbb;
        osg::ref_ptr<osg::Node> land_model =
            process_landuse(ltw, wbb, file_path);
        osg::ref_ptr<osg::Node> water_model = process_water(ltw, file_path);
        osg::ref_ptr<osg::Node> roads_model = process_roads(ltw, file_path);
        osg::ref_ptr<osg::Node> buildings_model =
            process_buildings(ltw, file_path);
        osg::ref_ptr<osg::Node> labels_model = process_labels(ltw, file_path);

        scene->addChild(land_model);
        scene->addChild(water_model);
        scene->addChild(roads_model);
        scene->addChild(buildings_model);
        root->addChild(labels_model);

        osg::Vec3d wtrans = wbb.center();
        wtrans.normalize();

        {
            osg::Vec3d toVec =
                osg::Matrix::rotate(osg::DegreesToRadians(-65.f), osg::Y_AXIS)
                    .preMult(osg::Z_AXIS);
            osg::Vec3d r = wtrans ^ toVec;
            r.normalize();
            wtrans = osg::Matrix::rotate(osg::DegreesToRadians(75.f), r)
                         .preMult(wtrans);
        }

        viewer->setLightingMode(osg::View::LightingMode::SKY_LIGHT);
        viewer->getLight()->setPosition(
            osg::Vec4(wtrans[0], wtrans[1], wtrans[2], 0.f));
        viewer->getLight()->setDirection(
            osg::Vec3(wtrans[0], wtrans[1], wtrans[2]));
        viewer->getLight()->setAmbient(osg::Vec4(0.2f, 0.2f, 0.2f, 1.0f));
        viewer->getLight()->setDiffuse(osg::Vec4(0.8f, 0.8f, 0.8f, 1.0f));
        viewer->getLight()->setSpecular(osg::Vec4(0.5f, 0.5f, 0.5f, 1.0f));

        root->setMatrix(ltw);
    };

    viewer->setSceneData(create_loading_screen());
    viewer->setUpViewOnSingleScreen(0);
    viewer->realize();

    std::future<void> loading =
        std::async(std::launch::async, prepare_scene, std::ref(root),
                   std::ref(scene), file_path);

    // viewport exists → safe to read size
    int w = viewer->getCamera()->getViewport()->width();
    int h = viewer->getCamera()->getViewport()->height();

    bool wasMoving = false;
    const float FADE_SPEED = 2.0f;
    double lastTime = viewer->getFrameStamp()->getReferenceTime();


    while (!viewer->done())
    {
        viewer->frame();

        if (loading.valid())
        {
            if (loading.wait_for(0ms) == std::future_status::ready)
            {
                loading.get();

                /**************/
                /** PPU SETUP */
                /**************/
                osg::ref_ptr<osgMap::postfx::PostProcessor> ppu =
                    new osgMap::postfx::PostProcessor(scene);
                {
                    ppu->pushLayer<osgMap::postfx::FXAA>();
                    ppu->pushLayer<osgMap::postfx::DOF>();
                    ppu->pushLayer<osgMap::postfx::Bloom>();

                    static_cast<osgMap::postfx::FXAA*>(
                        ppu->getLayer<osgMap::postfx::FXAA>())
                        ->setParameters(fxaa_params);
                    static_cast<osgMap::postfx::DOF*>(
                        ppu->getLayer<osgMap::postfx::DOF>())
                        ->setParameters(dof_params);
                    static_cast<osgMap::postfx::Bloom*>(
                        ppu->getLayer<osgMap::postfx::Bloom>())
                        ->setParameters(bloom_params);

                    viewer->addEventHandler(ppu->getResizeHandler());
                    viewer->addEventHandler(
                        ppu->getActivationHandler<osgMap::postfx::FXAA>(
                            osgGA::GUIEventAdapter::KeySymbol::KEY_1));
                    viewer->addEventHandler(
                        ppu->getActivationHandler<osgMap::postfx::DOF>(
                            osgGA::GUIEventAdapter::KeySymbol::KEY_2));
                    viewer->addEventHandler(
                        ppu->getActivationHandler<osgMap::postfx::Bloom>(
                            osgGA::GUIEventAdapter::KeySymbol::KEY_3));
                }

                root->addChild(ppu);
                root->addChild(ppu->getRenderPlaneProjection());
                ppu->resize(w, h);

                // Create HUD
                osg::Camera* hud = createHUD("images/logo.png", 0.3f, w, h);

                // Find the geode in the HUD (you might need to store it during
                // creation)
                osg::Geode* hudGeode =
                    dynamic_cast<osg::Geode*>(hud->getChild(0));

                // Add resize handler
                viewer->addEventHandler(new HUDResizeHandler(
                    hud, hudGeode, "images/logo.png", 0.3f));

                // Add HUD AFTER realize() (totally allowed)
                root->addChild(hud);
                viewer->setSceneData(root);

                // Initialize to visible
                g_currentAlpha = 1.0f;
                g_targetAlpha = 1.0f;

                // Set initial alpha values
                if (g_hudAlpha.valid())
                {
                    g_hudAlpha->set(g_currentAlpha);
                }

                lastTime = viewer->getFrameStamp()->getReferenceTime();
            }
        }
        else
        {
            double frameTime = viewer->getFrameStamp()->getReferenceTime();
            float deltaTime = frameTime - lastTime;
            lastTime = frameTime;

            auto* keySwitch = dynamic_cast<osgGA::KeySwitchMatrixManipulator*>(
                viewer->getCameraManipulator());

            if (keySwitch)
            {
                osgGA::CameraManipulator* current =
                    keySwitch->getCurrentMatrixManipulator();

                if (auto* google =
                        dynamic_cast<GoogleMapsManipulator*>(current))
                {
                    bool moving = google->isMoving();

                    // Update target alpha based on movement
                    if (moving)
                    {
                        g_targetAlpha = 0.0f; // Fade out when moving
                    }
                    else
                    {
                        g_targetAlpha = 1.0f; // Fade in when stopped

                        // When just stopped moving, update text content
                        if (wasMoving)
                        {
                            std::ostringstream ss;
                            osg::Vec3d hit, normal;
                            std::string landInfo =
                                getLandInfoAtIntersection(root, hit);
                            ss << "Land Data:\n" << landInfo;
                            hudSetText(ss.str());
                        }
                    }

                    wasMoving = moving;
                }
            }

            // ALWAYS smoothly interpolate current alpha toward target (runs
            // every frame)
            float diff = g_targetAlpha - g_currentAlpha;
            if (std::abs(diff) > 0.001f)
            {
                float step = FADE_SPEED * deltaTime;

                if (std::abs(diff) < step)
                {
                    g_currentAlpha = g_targetAlpha;
                }
                else
                {
                    g_currentAlpha += (diff > 0 ? step : -step);
                }

                // Clamp to valid range
                g_currentAlpha = std::max(0.0f, std::min(1.0f, g_currentAlpha));
            }

            // ALWAYS update alpha every frame for smooth animation
            if (g_hudAlpha.valid())
            {
                g_hudAlpha->set(g_currentAlpha);
            }
        }
    }

    return 0;
}