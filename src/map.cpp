#include <osgDB/ReadFile>
#include <osgUtil/Optimizer>
#include <osg/CoordinateSystemNode>

#include <osg/Switch>
#include <osg/Types>
#include <osgText/Text>
#include <osg/MatrixTransform>
#include <osg/ShapeDrawable>

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

#include "common.h"
#include "HUD.h"
#include "camera_manip.h"
#include "post_process.h"

using namespace osg;
float g_targetAlpha = 0.0f;
float g_currentAlpha = 0.0f;

osg::ref_ptr<osgViewer::Viewer> viewer;
osg::ref_ptr<osg::EllipsoidModel> ellipsoid;

int main(int argc, char** argv)
{
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


    {

        double maxTilt = 75.0;
        arguments.read("--max-tilt", maxTilt);

        osg::ref_ptr<osgGA::KeySwitchMatrixManipulator> keyswitchManipulator =
            new osgGA::KeySwitchMatrixManipulator;


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
        {
        }
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


        viewer->setCameraManipulator(keyswitchManipulator.get());
    }

    osgMap::postfx::FXAA::Parameters fxaa_params;
    osgMap::postfx::DOF::Parameters dof_params;
    osgMap::postfx::Bloom::Parameters bloom_params;
    {
        arguments.read("--fxaa-search-steps", fxaa_params.number_search_steps);
        arguments.read("--fxaa-blur-close", fxaa_params.blur_close_distance);
        arguments.read("--fxaa-blur-far", fxaa_params.blur_far_distance);
        arguments.read("--dof-max-blur", dof_params.max_blur);
        arguments.read("--dof-focus-range", dof_params.focus_range);
        arguments.read("--bloom-threshold", bloom_params.threshold);
        arguments.read("--bloom-intensity", bloom_params.intensity);
    }


    viewer->addEventHandler(new osgGA::StateSetManipulator(
        viewer->getCamera()->getOrCreateStateSet()));


    viewer->addEventHandler(new osgViewer::ThreadingHandler);


    viewer->addEventHandler(new osgViewer::WindowSizeHandler);

    viewer->addEventHandler(new osgViewer::StatsHandler);


    viewer->addEventHandler(
        new osgViewer::HelpHandler(arguments.getApplicationUsage()));


    viewer->addEventHandler(new osgViewer::RecordCameraPathHandler);


    viewer->addEventHandler(new osgViewer::LODScaleHandler);

    viewer->addEventHandler(new osgViewer::ScreenCaptureHandler);


    arguments.reportRemainingOptionsAsUnrecognized();


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


    osg::Matrixd ltw;
    osg::BoundingBox wbb;
    osg::ref_ptr<osg::Node> land_model = process_landuse(ltw, wbb, file_path);
    osg::ref_ptr<osg::Node> water_model = process_water(ltw, file_path);
    osg::ref_ptr<osg::Node> roads_model = process_roads(ltw, file_path);
    osg::ref_ptr<osg::Node> buildings_model = process_buildings(ltw, file_path);
    osg::ref_ptr<osg::Node> labels_model = process_labels(ltw, file_path);

    osg::ref_ptr<osg::Group> scene = new osg::Group;
    scene->addChild(land_model);
    scene->addChild(water_model);
    scene->addChild(roads_model);
    scene->addChild(buildings_model);

    osg::ref_ptr<osgMap::postfx::PostProcessor> ppu =
        new osgMap::postfx::PostProcessor(scene);
    {
        ppu->pushLayer<osgMap::postfx::FXAA>();
        ppu->pushLayer<osgMap::postfx::DOF>();
        ppu->pushLayer<osgMap::postfx::Bloom>();

        static_cast<osgMap::postfx::FXAA*>(
            ppu->getLayer<osgMap::postfx::FXAA>())
            ->setParameters(fxaa_params);
        static_cast<osgMap::postfx::DOF*>(ppu->getLayer<osgMap::postfx::DOF>())
            ->setParameters(dof_params);
        static_cast<osgMap::postfx::Bloom*>(
            ppu->getLayer<osgMap::postfx::Bloom>())
            ->setParameters(bloom_params);

        viewer->addEventHandler(ppu->getResizeHandler());
        viewer->addEventHandler(ppu->getActivationHandler<osgMap::postfx::FXAA>(
            osgGA::GUIEventAdapter::KeySymbol::KEY_1));
        viewer->addEventHandler(ppu->getActivationHandler<osgMap::postfx::DOF>(
            osgGA::GUIEventAdapter::KeySymbol::KEY_2));
        viewer->addEventHandler(
            ppu->getActivationHandler<osgMap::postfx::Bloom>(
                osgGA::GUIEventAdapter::KeySymbol::KEY_3));
    }

    osg::Vec3d wtrans = wbb.center();
    wtrans.normalize();
    viewer->setLightingMode(osg::View::LightingMode::SKY_LIGHT);
    viewer->getLight()->setPosition(
        osg::Vec4(wtrans[0], wtrans[1], wtrans[2], 0.f));
    viewer->getLight()->setDirection(
        osg::Vec3(wtrans[0], wtrans[1], wtrans[2]));
    viewer->getLight()->setAmbient(osg::Vec4(0.2f, 0.2f, 0.2f, 1.0f));
    viewer->getLight()->setDiffuse(osg::Vec4(0.8f, 0.8f, 0.8f, 1.0f));
    viewer->getLight()->setSpecular(osg::Vec4(0.5f, 0.5f, 0.5f, 1.0f));

    osg::MatrixTransform* root = new osg::MatrixTransform;
    root->setMatrix(ltw);
    root->addChild(ppu);
    root->addChild(ppu->getRenderPlaneProjection());


    viewer->setSceneData(root);
    viewer->setUpViewOnSingleScreen(0);


    viewer->realize();


    if (viewer->getCamera() == nullptr
        || viewer->getCamera()->getViewport() == nullptr)
    {
        std::cout << "Viewer setup failed!" << std::endl;
        return 0;
    }

    int w = viewer->getCamera()->getViewport()->width();
    int h = viewer->getCamera()->getViewport()->height();

    ppu->resize(w, h);


    osg::Camera* hud = createHUD("images/logo.png", 0.3f, w, h);

    osg::Geode* hudGeode = dynamic_cast<osg::Geode*>(hud->getChild(0));


    viewer->addEventHandler(
        new HUDResizeHandler(hud, hudGeode, "images/logo.png", 0.3f));


    root->addChild(hud);


    root->addChild(labels_model);


    bool wasMoving = false;
    const float FADE_SPEED = 2.0f;


    g_currentAlpha = 1.0f;
    g_targetAlpha = 1.0f;

    if (g_hudAlpha.valid())
    {
        g_hudAlpha->set(g_currentAlpha);
    }

    double lastTime = viewer->getFrameStamp()->getReferenceTime();

    while (!viewer->done())
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

            if (auto* google = dynamic_cast<GoogleMapsManipulator*>(current))
            {
                bool moving = google->isMoving();

                if (moving)
                {
                    g_targetAlpha = 0.0f;
                }
                else
                {
                    g_targetAlpha = 1.0f;


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


            g_currentAlpha = std::max(0.0f, std::min(1.0f, g_currentAlpha));
        }

        if (g_hudAlpha.valid())
        {
            g_hudAlpha->set(g_currentAlpha);
        }

        viewer->frame();
    }

    return 0;
}