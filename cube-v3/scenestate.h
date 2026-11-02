#ifndef SCENESTATE_H
#define SCENESTATE_H

#include <QQuaternion>
#include <QVector3D>
#include <QtMath>

#include <algorithm>
#include <cmath>

// Where the observer view is drawn. The order matches the items of the combo box in ControlPanel.
enum class ObserverMode
{
    Off,        // main camera only
    Inset,      // small view in the lower right corner of the main view
    Split,      // the widget is split in half (left/right or top/bottom, whichever keeps the halves wider)
};

// Everything that describes the scene, in the world frame.
// MainWidget owns one instance; ControlPanel and MainWindow only read it.
struct SceneState
{
    // Object pose (object frame w.r.t. world frame)
    QVector3D objectPosition;
    QQuaternion objectOrientation;

    // Camera (eye, look-at target and up vector in world frame)
    QVector3D cameraPosition;
    QVector3D cameraTarget;
    QVector3D cameraUp = QVector3D(0.0f, 1.0f, 0.0f);

    // Perspective projection of the camera
    float fieldOfView = 45.0f;  // vertical, degrees
    float nearPlane = 0.5f;
    float farPlane = 20.0f;

    // Automatic rotation of the object about the world y axis
    bool autoRotate = false;
    float autoRotateSpeed = 45.0f;  // degrees per second

    // Reference lines drawn in the world
    bool showGrid = true;           // grid on the world x-z plane
    bool showWorldAxes = true;
    bool showObjectAxes = true;

    // Observer camera: a second camera that orbits the world origin and shows the main camera from outside.
    // It is stored as orbit parameters directly, because its target is always the origin.
    ObserverMode observerMode = ObserverMode::Inset;
    float observerAzimuth;
    float observerElevation;
    float observerDistance;

    // Orbit parameters of the camera around the target, derived from (cameraPosition - cameraTarget).
    // azimuth: angle around the world y axis, 0 on the +z side, positive towards +x
    // elevation: angle above the world x-z plane
    float cameraDistance() const
    {
        return (cameraPosition - cameraTarget).length();
    }

    float cameraAzimuth() const
    {
        const QVector3D offset = cameraPosition - cameraTarget;
        return qRadiansToDegrees(std::atan2(offset.x(), offset.z()));
    }

    float cameraElevation() const
    {
        const QVector3D offset = cameraPosition - cameraTarget;
        const float distance = offset.length();
        if (distance <= 0.0f)
            return 0.0f;
        return qRadiansToDegrees(std::asin(std::clamp(offset.y() / distance, -1.0f, 1.0f)));
    }

    // Inverse of the three functions above: offset of the eye from the target for given orbit parameters
    static QVector3D orbitOffset(float azimuth, float elevation, float distance)
    {
        const float az = qDegreesToRadians(azimuth);
        const float el = qDegreesToRadians(elevation);
        return QVector3D(distance * std::cos(el) * std::sin(az),
                         distance * std::sin(el),
                         distance * std::cos(el) * std::cos(az));
    }
};

#endif // SCENESTATE_H
