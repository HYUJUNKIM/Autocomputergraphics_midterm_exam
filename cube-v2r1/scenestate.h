#ifndef SCENESTATE_H
#define SCENESTATE_H

#include <QQuaternion>
#include <QVector3D>
#include <QtMath>

#include <algorithm>
#include <cmath>

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

    // Automatic rotation of the object about the world y axis
    bool autoRotate = false;
    float autoRotateSpeed = 45.0f;  // degrees per second

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
};

#endif // SCENESTATE_H
