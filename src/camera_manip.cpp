#include "camera_manip.h"
#include <algorithm>

GoogleMapsManipulator::GoogleMapsManipulator()
    : _distance(100.0), _lastX(0), _lastY(0), _center(0, 0, 1),
      _isMoving(false), _lastMoveTime(0.0), _movementTimeout(0.2),
      _maxTiltDeg(75.0)
{}

bool GoogleMapsManipulator::isMoving() const
{
    double now = osg::Timer::instance()->time_s();
    return _isMoving && (now - _lastMoveTime < _movementTimeout);
}

void GoogleMapsManipulator::setMovementTimeout(double seconds) 
{ 
    _movementTimeout = seconds; 
}

void GoogleMapsManipulator::setMaxTiltDeg(double degrees) 
{ 
    _maxTiltDeg = std::max(0.0, std::min(90.0, degrees));
}

double GoogleMapsManipulator::getMaxTiltDeg() const 
{ 
    return _maxTiltDeg; 
}

void GoogleMapsManipulator::resetFromBounds()
{
    if (_node.valid())
    {
        const osg::BoundingSphere bs = _node->getBound();
        _center = bs.center();
        _distance = bs.radius() * 0.5;
    }
}

void GoogleMapsManipulator::setNode(osg::Node* node)
{
    _node = node;
    osgGA::CameraManipulator::setNode(node);
    resetFromBounds();
}

void GoogleMapsManipulator::home(double) 
{ 
    resetFromBounds(); 
}

void GoogleMapsManipulator::home(const osgGA::GUIEventAdapter&, osgGA::GUIActionAdapter&)
{
    resetFromBounds();
}

osg::Matrixd GoogleMapsManipulator::getInverseMatrix() const
{
    osg::Vec3d up = _center;
    up.normalize();
    osg::Vec3d east = osg::Vec3d(0, 0, 1) ^ up;
    if (east.length2() == 0.0)
    {
        east.set(1.0, 0.0, 0.0);
    }
    east.normalize();
    osg::Vec3d north = up ^ east;
    north.normalize();

    const double tiltRad = osg::DegreesToRadians(_tiltDeg);
    osg::Vec3d offset =
        (-north * std::sin(tiltRad)) + (up * std::cos(tiltRad));
    offset.normalize();
    osg::Vec3d eye = _center + offset * _distance;

    double earthRadius = 6371000.0;
    if (_node.valid())
    {
        double r = _node->getBound().radius();
        if (r > 1000.0)
        {
            earthRadius = r;
        }
    }

    double eyeDistanceFromOrigin = eye.length();
    double minEyeDistance = earthRadius + 10.0;
    
    if (eyeDistanceFromOrigin < minEyeDistance)
    {
        eye.normalize();
        eye *= minEyeDistance;
    }

    return osg::Matrixd::lookAt(eye, _center, north);
}

osg::Matrixd GoogleMapsManipulator::getMatrix() const
{
    return osg::Matrixd::inverse(getInverseMatrix());
}

void GoogleMapsManipulator::setByMatrix(const osg::Matrixd& matrix)
{
    osg::Vec3d eye = matrix.getTrans();

    if (eye.isNaN())
    {
        eye.set(0, 0, 100);
    }

    osg::Vec3d lookVector(-matrix(2, 0), -matrix(2, 1), -matrix(2, 2));
    lookVector.normalize();
    if (lookVector.isNaN())
    {
        lookVector.set(0, 0, -1);
    }

    osg::Vec3d localUp = eye;
    localUp.normalize();

    if (localUp.isNaN())
    {
        localUp.set(0, 0, 1);
    }

    osg::Vec3d localDown = -localUp;

    double dot = lookVector * localDown;
    if (dot > 1.0) dot = 1.0;
    if (dot < -1.0) dot = -1.0;
    _tiltDeg = osg::RadiansToDegrees(std::acos(dot));

    if (_tiltDeg < 0.0) _tiltDeg = 0.0;
    if (_tiltDeg > _maxTiltDeg) _tiltDeg = _maxTiltDeg;

    double earthRadius = 6371000.0;
    if (_node.valid())
    {
        double r = _node->getBound().center().length();
        if (r > 1000.0)
        {
            earthRadius = r;
        }
    }

    double a = 1.0;
    double b = 2.0 * (eye * lookVector);
    double c = (eye * eye) - (earthRadius * earthRadius);

    double discriminant = b * b - 4 * a * c;

    double t = -1.0;

    if (discriminant >= 0)
    {
        double t1 = (-b - std::sqrt(discriminant)) / (2.0 * a);
        double t2 = (-b + std::sqrt(discriminant)) / (2.0 * a);

        if (t1 > 0 && t2 > 0)
            t = std::min(t1, t2);
        else if (t1 > 0)
            t = t1;
        else if (t2 > 0)
            t = t2;
    }

    double minDistance = 10.0;

    if (t > 0)
    {
        osg::Vec3d groundPoint = eye + lookVector * t;
        _center = groundPoint;
        _distance = std::max(t, minDistance);
    }
    else
    {
        resetFromBounds();
    }
}

void GoogleMapsManipulator::setByInverseMatrix(const osg::Matrixd& matrix)
{
    setByMatrix(osg::Matrixd::inverse(matrix));
}

bool GoogleMapsManipulator::handle(const osgGA::GUIEventAdapter& ea,
            osgGA::GUIActionAdapter& aa)
{
    auto markMovement = [&]() {
        _isMoving = true;
        _lastMoveTime = osg::Timer::instance()->time_s();
    };

    if (ea.getEventType() == osgGA::GUIEventAdapter::PUSH)
    {
        _lastX = ea.getXnormalized();
        _lastY = ea.getYnormalized();
        markMovement();
        return true;
    }

    if (ea.getEventType() == osgGA::GUIEventAdapter::DRAG
        && (ea.getButtonMask()
            & (osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON
               | osgGA::GUIEventAdapter::RIGHT_MOUSE_BUTTON)))
    {
        float x = ea.getXnormalized();
        float y = ea.getYnormalized();
        const float dx = x - _lastX;
        const float dy = y - _lastY;

        if (ea.getButtonMask() & osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
        {
            osg::Vec3d up = _center;
            up.normalize();

            osg::Vec3d east = osg::Vec3d(0, 0, 1) ^ up;
            if (east.length2() == 0.0) east.set(1.0, 0.0, 0.0);
            east.normalize();

            osg::Vec3d north = up ^ east;
            north.normalize();

            _center -= (east * dx * _distance) + (north * dy * _distance);
        }

        if (ea.getButtonMask() & osgGA::GUIEventAdapter::RIGHT_MOUSE_BUTTON)
        {
            const double sensitivity = 100.0;
            _tiltDeg -= dy * sensitivity;

            if (_tiltDeg < 0.0) _tiltDeg = 0.0;
            if (_tiltDeg > _maxTiltDeg) _tiltDeg = _maxTiltDeg;
        }

        _lastX = x;
        _lastY = y;

        markMovement();
        aa.requestRedraw();
        return true;
    }

    if (ea.getEventType() == osgGA::GUIEventAdapter::SCROLL)
    {
        _distance *=
            (ea.getScrollingMotion() == osgGA::GUIEventAdapter::SCROLL_UP
                 ? 0.8
                 : 1.25);
        _distance = std::max(_distance, 10.0);

        markMovement();
        aa.requestRedraw();
        return true;
    }

    if (ea.getEventType() == osgGA::GUIEventAdapter::KEYDOWN
        && ea.getKey() == osgGA::GUIEventAdapter::KEY_Home)
    {
        resetFromBounds();
        markMovement();
        aa.requestRedraw();
        return true;
    }

    return false;
}
