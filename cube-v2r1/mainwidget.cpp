// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#include "mainwidget.h"

#include <QDebug>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

// Window size; initial values are used as the size of the window at startup
static int g_windowWidth = 512;
static int g_windowHeight = 512;

// Limits that keep lookAt() well defined
static const float kMinDistance = 1.0f;     // camera never reaches the target point
static const float kMaxDistance = 50.0f;
static const float kMaxElevation = 89.0f;   // up vector is world y, so never look straight up or down

// Interaction speeds
static const float kDegreesPerPixel = 0.5f;        // object rotation (left drag), same as cube-v2
static const float kOrbitDegreesPerPixel = 0.4f;   // camera orbit (right drag / Shift + left drag)
static const float kOrbitDegreesPerKey = 3.0f;     // camera orbit (arrow keys)
static const float kMoveStep = 0.2f;               // camera move per key press (W A S D Q E)
static const float kZoomPerNotch = 0.9f;           // one wheel notch scales the camera distance by this

QSize MainWidget::sizeHint() const
{
    return QSize(g_windowWidth, g_windowHeight);
}

MainWidget::MainWidget(QWidget *parent)
    : QOpenGLWidget(parent)
{
    // Take keyboard focus when clicked, so W A S D and the arrow keys reach keyPressEvent()
    setFocusPolicy(Qt::StrongFocus);

    // The scene does not need OpenGL, so it is ready before initializeGL() and the panel can show it
    initScene();

    // Auto rotation runs at about 60 Hz
    animationTimer.setInterval(16);
    animationTimer.setTimerType(Qt::PreciseTimer);
    connect(&animationTimer, &QTimer::timeout, this, &MainWidget::stepAutoRotation);
}

MainWidget::~MainWidget()
{
    // Make sure the context is current when deleting the texture
    // and the buffers.
    makeCurrent();
    delete texture;
    delete geometries;
    doneCurrent();
}

//! [0]
void MainWidget::mousePressEvent(QMouseEvent *e)
{
    // Save mouse press position
    lastMousePosition = QVector2D(e->position());
}

void MainWidget::mouseMoveEvent(QMouseEvent *e)
{
    // Mouse movement since the last event
    QVector2D pos = QVector2D(e->position());
    QVector2D diff = pos - lastMousePosition;
    lastMousePosition = pos;

    if (diff.isNull())
        return;

    const bool orbit = (e->buttons() & Qt::RightButton)
                    || ((e->buttons() & Qt::LeftButton) && (e->modifiers() & Qt::ShiftModifier));

    if (orbit) {
        // Dragging right moves the camera to its left around the target, dragging up moves it down,
        // so the scene turns in the drag direction like the object rotation does
        orbitCamera(-diff.x() * kOrbitDegreesPerPixel, diff.y() * kOrbitDegreesPerPixel);
    } else if (e->buttons() & Qt::LeftButton) {
        rotateObject(diff);
    }
}

void MainWidget::rotateObject(const QVector2D &diff)
{
    // Convert Mouse movement to opengl view coordinates
    const float dx = diff.x();
    const float dy = -diff.y();
    // Rotation axis of view window is perpendicular to
    // the mouse position difference vector
    QVector3D n_view = QVector3D(-dy, dx, 0.0f).normalized();

    // Rotation angle is proportional to the length of the mouse movement
    float angle = diff.length() * kDegreesPerPixel;

    // view frame -> world frame : n_world = R_cam * n_view
    // The camera pose is computed from the current state (cube-v2 used the matrix of the last paint)
    QVector3D n_world = viewMatrix().inverted().mapVector(n_view);

    // world frame -> object frame : n_object = Ro^-1 * n_world, with the current orientation
    QVector3D n_object = scene.objectOrientation.inverted().rotatedVector(n_world).normalized();

    // Rotate the object frame about its own axis n_object:
    // Mo' = To x Ro x R(n_object), so the new rotation is post-multiplied
    scene.objectOrientation = (scene.objectOrientation * QQuaternion::fromAxisAndAngle(n_object, angle)).normalized();

    notifySceneChanged();
}
//! [0]

void MainWidget::wheelEvent(QWheelEvent *e)
{
    // One notch is 120 units; turning the wheel away from the user moves the camera closer
    const float notches = e->angleDelta().y() / 120.0f;
    if (notches == 0.0f)
        return;

    const SceneState &s = scene;
    setCameraOrbit(s.cameraAzimuth(), s.cameraElevation(),
                   s.cameraDistance() * std::pow(kZoomPerNotch, notches));
    e->accept();
}

void MainWidget::keyPressEvent(QKeyEvent *e)
{
    switch (e->key()) {
    // Move the camera (eye and target together) in its own frame
    case Qt::Key_W: moveCamera(0.0f, 0.0f, kMoveStep); break;
    case Qt::Key_S: moveCamera(0.0f, 0.0f, -kMoveStep); break;
    case Qt::Key_A: moveCamera(-kMoveStep, 0.0f, 0.0f); break;
    case Qt::Key_D: moveCamera(kMoveStep, 0.0f, 0.0f); break;
    case Qt::Key_E: moveCamera(0.0f, kMoveStep, 0.0f); break;
    case Qt::Key_Q: moveCamera(0.0f, -kMoveStep, 0.0f); break;
    // Orbit the camera around the target
    case Qt::Key_Left:  orbitCamera(-kOrbitDegreesPerKey, 0.0f); break;
    case Qt::Key_Right: orbitCamera(kOrbitDegreesPerKey, 0.0f); break;
    case Qt::Key_Up:    orbitCamera(0.0f, kOrbitDegreesPerKey); break;
    case Qt::Key_Down:  orbitCamera(0.0f, -kOrbitDegreesPerKey); break;
    case Qt::Key_Space: setAutoRotate(!scene.autoRotate); break;
    case Qt::Key_R:     resetScene(); break;
    default:
        QOpenGLWidget::keyPressEvent(e);
        return;
    }
}

void MainWidget::initializeGL()
{
    if (!initializeOpenGLFunctions())
        qFatal("Failed to initialize OpenGL 3.3 Core functions");

    const QSurfaceFormat actualFormat = context()->format();
    qInfo() << "OpenGL context:" << actualFormat.majorVersion()
            << actualFormat.minorVersion() << actualFormat.profile();

    glClearColor(0, 0, 0, 1);

    initShaders();
    initTextures();

    geometries = new GeometryEngine;
}

// Initial object and camera poses in the world frame (same as cube-v2)
void MainWidget::initScene()
{
    // Object frame: at the world origin, rotated -90 degrees about the world y axis
    scene.objectPosition    = QVector3D(0.0f, 0.0f, 0.0f);
    scene.objectOrientation = QQuaternion::fromAxisAndAngle(0.0f, 1.0f, 0.0f, -90.0f);

    // Camera: at (0, 0, 5) looking towards the origin, world y axis is up
    scene.cameraPosition = QVector3D(0.0f, 0.0f, 5.0f);
    scene.cameraTarget   = QVector3D(0.0f, 0.0f, 0.0f);
    scene.cameraUp       = QVector3D(0.0f, 1.0f, 0.0f);
}

//! [3]
void MainWidget::initShaders()
{
    // Compile vertex shader
    if (!program.addShaderFromSourceFile(QOpenGLShader::Vertex, ":/vshader.glsl"))
        qFatal("Vertex shader compilation failed:\n%s", qPrintable(program.log()));

    // Compile fragment shader
    if (!program.addShaderFromSourceFile(QOpenGLShader::Fragment, ":/fshader.glsl"))
        qFatal("Fragment shader compilation failed:\n%s", qPrintable(program.log()));

    // Link shader pipeline
    if (!program.link())
        qFatal("Shader program link failed:\n%s", qPrintable(program.log()));

    // Bind shader pipeline for use
    if (!program.bind())
        qFatal("Shader program bind failed:\n%s", qPrintable(program.log()));
}
//! [3]

//! [4]
void MainWidget::initTextures()
{
    // Load cube.png image
    texture = new QOpenGLTexture(QImage(":/cube.png").flipped());

    // Set nearest filtering mode for texture minification
    texture->setMinificationFilter(QOpenGLTexture::Nearest);

    // Set bilinear filtering mode for texture magnification
    texture->setMagnificationFilter(QOpenGLTexture::Linear);

    // Wrap texture coordinates by repeating
    // f.ex. texture coordinate (1.1, 1.2) is same as (0.1, 0.2)
    texture->setWrapMode(QOpenGLTexture::Repeat);
}
//! [4]

//! [5]
void MainWidget::resizeGL(int w, int h)
{
    // Keep track of the current window size
    g_windowWidth = w;
    g_windowHeight = h;

    // Calculate aspect ratio
    qreal aspect = qreal(g_windowWidth) / qreal(g_windowHeight ? g_windowHeight : 1);

    // Near and far planes are wider than in cube-v2 (3.0 and 7.0) because the camera
    // distance can now change between kMinDistance and kMaxDistance
    const qreal zNear = 0.1, zFar = 100.0, fov = 45.0;

    // Reset projection
    projection.setToIdentity();

    // Set perspective projection
    projection.perspective(fov, aspect, zNear, zFar);
}
//! [5]

QMatrix4x4 MainWidget::viewMatrix() const
{
    // View matrix from camera eye, target and up vector (same as gluLookAt / glm::lookAt)
    QMatrix4x4 viewmatrix;
    viewmatrix.lookAt(scene.cameraPosition, scene.cameraTarget, scene.cameraUp);
    return viewmatrix;
}

void MainWidget::paintGL()
{
    // Clear color and depth buffer
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

//! [2]
    // Enable depth buffer
    glEnable(GL_DEPTH_TEST);

    // Enable back face culling
    glEnable(GL_CULL_FACE);
//! [2]

    texture->bind();
    program.bind();

//! [6]
    // Calculate model transformation, which includes translation and rotation of the cube
    // Model transformation: translation followed by rotation, Mo = To x Ro
    QMatrix4x4 modelmatrix;
    modelmatrix.translate(scene.objectPosition);     // Mo = To
    modelmatrix.rotate(scene.objectOrientation);     // Mo = To x Ro

    // Set modelview matrix as the combination of camera and object transformations
    QMatrix4x4 modelviewmatrix = viewMatrix() * modelmatrix;

    // Set modelview-projection matrix
    program.setUniformValue("mvp_matrix", projection * modelviewmatrix);
//! [6]

    // Use texture unit 0 which contains cube.png
    program.setUniformValue("cube_texture", 0);

    // Draw cube geometry
    geometries->drawCubeGeometry(&program);

    ++frameCount;
}

int MainWidget::takeFrameCount()
{
    const int frames = frameCount;
    frameCount = 0;
    return frames;
}

void MainWidget::notifySceneChanged()
{
    // Request an update and let the panel and the status bar follow the new state
    update();
    emit sceneChanged();
}

void MainWidget::setObjectPosition(const QVector3D &position)
{
    scene.objectPosition = position;
    notifySceneChanged();
}

void MainWidget::setObjectEulerAngles(const QVector3D &pitchYawRoll)
{
    // Qt's convention: rotation about z (roll), then x (pitch), then y (yaw), all in degrees
    scene.objectOrientation = QQuaternion::fromEulerAngles(pitchYawRoll);
    notifySceneChanged();
}

void MainWidget::setCameraOrbit(float azimuth, float elevation, float distance)
{
    // Keep the azimuth in [-180, 180] and stay away from the poles and from the target
    azimuth = std::remainder(azimuth, 360.0f);
    elevation = std::clamp(elevation, -kMaxElevation, kMaxElevation);
    distance = std::clamp(distance, kMinDistance, kMaxDistance);

    // Spherical coordinates around the target: azimuth 0 is the +z side, elevation 0 is the x-z plane
    const float az = qDegreesToRadians(azimuth);
    const float el = qDegreesToRadians(elevation);
    const QVector3D offset(distance * std::cos(el) * std::sin(az),
                           distance * std::sin(el),
                           distance * std::cos(el) * std::cos(az));
    scene.cameraPosition = scene.cameraTarget + offset;
    notifySceneChanged();
}

void MainWidget::orbitCamera(float deltaAzimuth, float deltaElevation)
{
    // Only the camera moves; the object and the target stay where they are
    setCameraOrbit(scene.cameraAzimuth() + deltaAzimuth,
                   scene.cameraElevation() + deltaElevation,
                   scene.cameraDistance());
}

void MainWidget::moveCamera(float right, float up, float forward)
{
    // Translate eye and target together, so the viewing direction does not change
    const QVector3D f = (scene.cameraTarget - scene.cameraPosition).normalized();
    const QVector3D r = QVector3D::crossProduct(f, scene.cameraUp).normalized();
    const QVector3D delta = r * right + scene.cameraUp * up + f * forward;
    scene.cameraPosition += delta;
    scene.cameraTarget += delta;
    notifySceneChanged();
}

void MainWidget::setCameraPosition(const QVector3D &position)
{
    // Go through the orbit parameters so the distance and elevation limits are applied
    scene.cameraPosition = position;
    setCameraOrbit(scene.cameraAzimuth(), scene.cameraElevation(), scene.cameraDistance());
}

void MainWidget::setCameraTarget(const QVector3D &target)
{
    // The camera stays in place and turns towards the new target
    scene.cameraTarget = target;
    setCameraPosition(scene.cameraPosition);
}

void MainWidget::lookAtObject()
{
    setCameraTarget(scene.objectPosition);
}

void MainWidget::setAutoRotate(bool enabled)
{
    if (scene.autoRotate == enabled)
        return;

    scene.autoRotate = enabled;
    if (enabled) {
        animationClock.start();
        animationTimer.start();
    } else {
        animationTimer.stop();
    }
    notifySceneChanged();
}

void MainWidget::setAutoRotateSpeed(float degreesPerSecond)
{
    scene.autoRotateSpeed = degreesPerSecond;
    notifySceneChanged();
}

void MainWidget::stepAutoRotation()
{
    // Angle from the real elapsed time, so the speed does not depend on the frame rate
    const float seconds = animationClock.restart() / 1000.0f;

    // Spin in place about the world y axis: pre-multiplied, so the axis is in the world frame
    const QQuaternion step = QQuaternion::fromAxisAndAngle(0.0f, 1.0f, 0.0f, scene.autoRotateSpeed * seconds);
    scene.objectOrientation = (step * scene.objectOrientation).normalized();
    notifySceneChanged();
}

void MainWidget::resetScene()
{
    // Poses go back to the initial values; the auto rotation settings are kept
    initScene();
    notifySceneChanged();
}
