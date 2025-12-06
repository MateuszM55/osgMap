#include <osgGA/CameraManipulator>
#include <osgGA/GUIEventAdapter>
#include <osgGA/GUIActionAdapter>
#include <osg/Matrix>
#include <osg/Vec3d>
#include <osg/BoundingSphere>
#include <osg/Node>
#include <algorithm>

class GoogleMapsManipulator : public osgGA::CameraManipulator {
public:
    GoogleMapsManipulator()
        : _distance(100.0), _lastX(0), _lastY(0), _center(0, 0, 1),
          _isMoving(false), _lastMoveTime(0.0), _movementTimeout(0.2)
    {}

    bool isMoving() const
    {
        double now = osg::Timer::instance()->time_s();
        return _isMoving && (now - _lastMoveTime < _movementTimeout);
    }

    void setMovementTimeout(double seconds) { _movementTimeout = seconds; }

    void resetFromBounds()
    {
        if (_node.valid())
        {
            const osg::BoundingSphere bs = _node->getBound();
            _center = bs.center();
            _distance = bs.radius() * 0.5;
        }
    }

    void setNode(osg::Node* node) override
    {
        _node = node;
        osgGA::CameraManipulator::setNode(node);
        resetFromBounds();
    }
    void home(double) override { resetFromBounds(); }
    void home(const osgGA::GUIEventAdapter&, osgGA::GUIActionAdapter&) override
    {
        resetFromBounds();
    }

    // Build the view matrix each frame: eye along the center direction at a given distance, global Z as up.
    osg::Matrixd getInverseMatrix() const override
    {
        // Local tangent frame at `_center`: up is radial from origin, east is Z x up, north is up x east.
        osg::Vec3d up = _center; // radial up
        up.normalize();
        osg::Vec3d east = osg::Vec3d(0, 0, 1) ^ up;
        if (east.length2() == 0.0) {
            // Avoid degeneracy at poles: pick arbitrary east
            east.set(1.0, 0.0, 0.0);
        }
        east.normalize();
        osg::Vec3d north = up ^ east; // geographic north
        north.normalize();

        // Eye lies in the plane spanned by local north and up, controlled by tilt angle [45,90] deg.
        const double tiltRad = osg::DegreesToRadians(_tiltDeg);
        osg::Vec3d offset = (-north * std::sin(tiltRad)) + (up * std::cos(tiltRad));
        offset.normalize();
        osg::Vec3d eye = _center + offset * _distance;
        // Keep screen up aligned to geographic north (no yaw/rotation).
        return osg::Matrixd::lookAt(eye, _center, north);
    }

    // Forward matrix required by the base class.
    osg::Matrixd getMatrix() const override
    {
        return osg::Matrixd::inverse(getInverseMatrix());
    }

    void setByMatrix(const osg::Matrixd& matrix) override
    {
        // 1. Get Camera Physical Position (Eye)
        osg::Vec3d eye = matrix.getTrans();

        // 2. Get Look Vector (Negative Z of the matrix rotation)
        osg::Vec3d lookVector(-matrix(2, 0), -matrix(2, 1), -matrix(2, 2));
        lookVector.normalize();

        // 3. Define "Local Up" and "Down"
        // In ECEF, Up is the vector from Earth Center (0,0,0) to the Eye.
        osg::Vec3d localUp = eye;
        localUp.normalize();
        osg::Vec3d localDown = -localUp;

        // 4. Calculate Tilt
        // Angle between LookVector and LocalDown (Gravity vector)
        double dot = lookVector * localDown;
        if (dot > 1.0) dot = 1.0;
        if (dot < -1.0) dot = -1.0;
        _tiltDeg = osg::RadiansToDegrees(std::acos(dot));

        // Clamp tilt to valid range
        if (_tiltDeg < 0.0) _tiltDeg = 0.0;
        if (_tiltDeg > 90.0) _tiltDeg = 90.0;

        // 5. Calculate Center (Intersection with the Globe)
        // We need the radius of the earth/city surface.
        // If we have a node, use its distance from origin. Otherwise assume
        // standard Earth radius.
        double earthRadius = 6371000.0; // Default meters
        if (_node.valid())
        {
            // The center of the city model tells us the radius at this location
            earthRadius = _node->getBound().center().length();
        }

        // Ray-Sphere Intersection Math
        // Ray: P = Eye + t * LookVector
        // Sphere: |P|^2 = R^2
        // Substitute: |Eye + t*L|^2 = R^2
        // Expands to Quadratic: a*t^2 + b*t + c = 0

        // a = L . L (which is 1.0 because lookVector is normalized)
        double a = 1.0;
        // b = 2 * (Eye . L)
        double b = 2.0 * (eye * lookVector);
        // c = (Eye . Eye) - R^2
        double c = (eye * eye) - (earthRadius * earthRadius);

        double discriminant = b * b - 4 * a * c;

        if (discriminant >= 0)
        {
            // Two intersections (entering and exiting the earth).
            // We want the closest one (smallest positive t).
            double t1 = (-b - std::sqrt(discriminant)) / (2.0 * a);
            double t2 = (-b + std::sqrt(discriminant)) / (2.0 * a);

            double t = -1.0;
            if (t1 > 0 && t2 > 0)
                t = std::min(t1, t2);
            else if (t1 > 0)
                t = t1;
            else if (t2 > 0)
                t = t2;

            if (t > 0)
            {
                _distance = t;
                _center = eye + lookVector * _distance;
            }
            else
            {
                // Intersection is behind us? Fallback.
                // Just project 1000m in front.
                _distance = 1000.0;
                _center = eye + lookVector * _distance;
            }
        }
        else
        {
            // Ray misses the Earth entirely (looking at space).
            // Create a fake center point at the horizon distance
            _distance = 10000.0; // Arbitrary horizon
            _center = eye + lookVector * _distance;
        }
    }

    void setByInverseMatrix(const osg::Matrixd& matrix) override
    {
        setByMatrix(osg::Matrixd::inverse(matrix));
    }

    bool handle(const osgGA::GUIEventAdapter& ea,
                osgGA::GUIActionAdapter& aa) override
    {
        // Record movement helpers
        // Keeps track of activity for inertia or idle timers
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

            // Panning
            if (ea.getButtonMask() & osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
            {
                // Pan: move center along local east/north, maintaining north-up
                // orientation.
                osg::Vec3d up = _center;
                up.normalize();

                osg::Vec3d east = osg::Vec3d(0, 0, 1) ^ up;
                // Handle case where up vector is
                // effectively vertical
                if (east.length2() == 0.0) east.set(1.0, 0.0, 0.0);
                east.normalize();

                osg::Vec3d north = up ^ east;
                north.normalize(); // Explicit
                                   // normalization

                _center -= (east * dx * _distance) + (north * dy * _distance);
            }

            // [Feature from Snippet 2] Tilting
            if (ea.getButtonMask() & osgGA::GUIEventAdapter::RIGHT_MOUSE_BUTTON)
            {
                // Tilt without rotation: adjust tilt angle based on vertical
                // drag. Positive dy (drag up) decreases tilt towards 0 (more
                // top-down).
                const double sensitivity = 100.0; // scale dy to degrees
                _tiltDeg -= dy * sensitivity;

                // Clamp limits
                if (_tiltDeg < 0.0) _tiltDeg = 0.0; // top-down limit
                if (_tiltDeg > 45.0) _tiltDeg = 45.0; // flat limit
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

        // Home key resets center/distance to node bounds.
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

private:
private:
    osg::observer_ptr<osg::Node> _node;
    osg::Vec3d _center;
    double _distance;
    float _lastX, _lastY;
    double _tiltDeg;

    bool _isMoving;
    double _lastMoveTime;
    double _movementTimeout;
};






