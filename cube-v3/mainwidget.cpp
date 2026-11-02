// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#include "mainwidget.h"

#include <QDebug>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QPainter>
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

// Limits of the projection settings
static const float kMinFieldOfView = 10.0f;
static const float kMaxFieldOfView = 120.0f;
static const float kMinNear = 0.1f;
static const float kMaxNear = 10.0f;
static const float kMinDepthRange = 1.0f;   // far is at least this much behind near
static const float kMaxFar = 200.0f;

// Observer camera: orbits the world origin, far enough to see the whole frustum of the main camera
static const float kMinObserverDistance = 5.0f;
static const float kMaxObserverDistance = 200.0f;
static const float kObserverFieldOfView = 45.0f;
static const float kObserverNear = 0.5f;
static const float kObserverFar = 1000.0f;

// Layout of the views and reference lines
static const int kInsetMargin = 12;                 // gap between the inset view and the widget edges
static const int kGridHalfCount = 10;               // grid lines from -10 to 10 on the x-z plane
static const float kGridSpacing = 1.0f;
static const float kObjectAxesLength = 1.8f;        // the cube spans [-1, 1], so the axes stick out of it
static const float kCameraAxesLength = 1.5f;
static const float kTargetMarkerSize = 0.2f;

// Interaction speeds
static const float kDegreesPerPixel = 0.5f;        // object rotation (left drag), same as cube-v2
static const float kOrbitDegreesPerPixel = 0.4f;   // camera orbit (right drag / Shift + left drag)
static const float kOrbitDegreesPerKey = 3.0f;     // camera orbit (arrow keys)
static const float kMoveStep = 0.2f;               // camera move per key press (W A S D Q E)
static const float kZoomPerNotch = 0.9f;           // one wheel notch scales the camera distance by this

static float aspectRatio(const QRect &area)
{
    return float(area.width()) / float(area.height() ? area.height() : 1);
}

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
    delete grid;
    delete worldAxes;
    delete localAxes;
    delete targetMarker;
    delete viewPyramid;
    delete frustum;
    doneCurrent();
}

//! [0]
void MainWidget::mousePressEvent(QMouseEvent *e)
{
    // Save mouse press position
    lastMousePosition = QVector2D(e->position());

    // A drag that starts in the observer view moves the observer camera until the button is released
    draggingObserver = observerViewport().contains(e->position().toPoint());
}

void MainWidget::mouseMoveEvent(QMouseEvent *e)
{
    // Mouse movement since the last event
    QVector2D pos = QVector2D(e->position());
    QVector2D diff = pos - lastMousePosition;
    lastMousePosition = pos;

    if (diff.isNull())
        return;

    if (draggingObserver) {
        // Any button: orbit the observer around the world origin, in the same directions as the main camera
        setObserverOrbit(scene.observerAzimuth - diff.x() * kOrbitDegreesPerPixel,
                         scene.observerElevation + diff.y() * kOrbitDegreesPerPixel,
                         scene.observerDistance);
        return;
    }

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
    const float scale = std::pow(kZoomPerNotch, notches);
    if (observerViewport().contains(e->position().toPoint()))
        setObserverOrbit(s.observerAzimuth, s.observerElevation, s.observerDistance * scale);
    else
        setCameraOrbit(s.cameraAzimuth(), s.cameraElevation(), s.cameraDistance() * scale);
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
    // Observer view: off -> inset -> split -> off
    case Qt::Key_V:     setObserverMode(ObserverMode((int(scene.observerMode) + 1) % 3)); break;
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

    initShaders();
    initTextures();

    geometries = new GeometryEngine;

    // Reference lines: x and z axes span the grid, the y axis is shorter
    grid = new LineGeometry(LineGeometry::grid(kGridHalfCount, kGridSpacing));
    worldAxes = new LineGeometry(LineGeometry::worldAxes(QVector3D(kGridHalfCount, 3.0f, kGridHalfCount) * kGridSpacing));
    localAxes = new LineGeometry(LineGeometry::axes());
    targetMarker = new LineGeometry(LineGeometry::star(QVector3D(1.0f, 1.0f, 1.0f)));
    viewPyramid = new LineGeometry(LineGeometry::pyramid(QVector3D(0.55f, 0.55f, 0.55f)));
    frustum = new LineGeometry(LineGeometry::box(QVector3D(1.0f, 0.85f, 0.2f)));
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

    // Observer: above and to the right of the main camera, far enough to see its frustum
    scene.observerAzimuth   = 50.0f;
    scene.observerElevation = 30.0f;
    scene.observerDistance  = 40.0f;
}

//! [3]
// Compiles and links one shader program; any error stops the program with the shader log
static void buildProgram(QOpenGLShaderProgram &program, const QString &vertexFile, const QString &fragmentFile)
{
    // Compile vertex shader
    if (!program.addShaderFromSourceFile(QOpenGLShader::Vertex, vertexFile))
        qFatal("Vertex shader compilation failed:\n%s", qPrintable(program.log()));

    // Compile fragment shader
    if (!program.addShaderFromSourceFile(QOpenGLShader::Fragment, fragmentFile))
        qFatal("Fragment shader compilation failed:\n%s", qPrintable(program.log()));

    // Link shader pipeline
    if (!program.link())
        qFatal("Shader program link failed:\n%s", qPrintable(program.log()));
}

void MainWidget::initShaders()
{
    // Textured cube
    buildProgram(program, ":/vshader.glsl", ":/fshader.glsl");

    // Colored lines (grid, axes, camera and frustum)
    buildProgram(lineProgram, ":/line_vshader.glsl", ":/line_fshader.glsl");
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
    // Keep track of the current window size.
    // The projection is no longer set here: it depends on the observer layout (the main view may be
    // only half of the widget) and on the panel settings, so projectionMatrix() computes it every frame.
    g_windowWidth = w;
    g_windowHeight = h;
}
//! [5]

QMatrix4x4 MainWidget::modelMatrix() const
{
    // Model transformation: translation followed by rotation, Mo = To x Ro
    QMatrix4x4 modelmatrix;
    modelmatrix.translate(scene.objectPosition);     // Mo = To
    modelmatrix.rotate(scene.objectOrientation);     // Mo = To x Ro
    return modelmatrix;
}

QMatrix4x4 MainWidget::viewMatrix() const
{
    // View matrix from camera eye, target and up vector (same as gluLookAt / glm::lookAt)
    QMatrix4x4 viewmatrix;
    viewmatrix.lookAt(scene.cameraPosition, scene.cameraTarget, scene.cameraUp);
    return viewmatrix;
}

QMatrix4x4 MainWidget::projectionMatrix() const
{
    // Perspective projection with the aspect ratio of the area the main view is drawn in
    QMatrix4x4 projection;
    projection.perspective(scene.fieldOfView, aspectRatio(mainViewport()), scene.nearPlane, scene.farPlane);
    return projection;
}

QMatrix4x4 MainWidget::observerViewMatrix() const
{
    // The observer orbits the world origin, the same way the main camera orbits its target
    const QVector3D eye = SceneState::orbitOffset(scene.observerAzimuth, scene.observerElevation,
                                                  scene.observerDistance);
    QMatrix4x4 viewmatrix;
    viewmatrix.lookAt(eye, QVector3D(0.0f, 0.0f, 0.0f), QVector3D(0.0f, 1.0f, 0.0f));
    return viewmatrix;
}

QMatrix4x4 MainWidget::observerProjectionMatrix() const
{
    // Fixed projection with a far plane beyond any frustum the main camera can have
    QMatrix4x4 projection;
    projection.perspective(kObserverFieldOfView, aspectRatio(observerViewport()), kObserverNear, kObserverFar);
    return projection;
}

QRect MainWidget::mainViewport() const
{
    if (scene.observerMode != ObserverMode::Split)
        return rect();

    // Left half of a wide widget, top half of a tall one
    if (width() >= height())
        return QRect(0, 0, width() / 2, height());
    return QRect(0, 0, width(), height() / 2);
}

QRect MainWidget::observerViewport() const
{
    switch (scene.observerMode) {
    case ObserverMode::Inset: {
        // Lower right corner, 40% of the width and height of the widget
        const int w = width() * 2 / 5;
        const int h = height() * 2 / 5;
        return QRect(width() - w - kInsetMargin, height() - h - kInsetMargin, w, h);
    }
    case ObserverMode::Split: {
        // The other half of mainViewport()
        const QRect main = mainViewport();
        if (width() >= height())
            return QRect(main.width(), 0, width() - main.width(), height());
        return QRect(0, main.height(), width(), height() - main.height());
    }
    case ObserverMode::Off:
        break;
    }
    return QRect();
}

void MainWidget::setGLViewport(const QRect &area)
{
    // Widget coordinates are logical pixels with y pointing down;
    // OpenGL uses device pixels (2x on a Retina display) with y pointing up
    const qreal ratio = devicePixelRatioF();
    const int x = qRound(area.x() * ratio);
    const int y = qRound((height() - area.y() - area.height()) * ratio);
    const int w = qRound(area.width() * ratio);
    const int h = qRound(area.height() * ratio);

    // The scissor box is only used while GL_SCISSOR_TEST is enabled, to clear this area alone
    glViewport(x, y, w, h);
    glScissor(x, y, w, h);
}

void MainWidget::paintGL()
{
    // Clear color and depth buffer (the clear color is set every frame, the observer view uses another one)
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

//! [2]
    // Enable depth buffer
    glEnable(GL_DEPTH_TEST);

    // Enable back face culling
    glEnable(GL_CULL_FACE);
//! [2]

    // Main camera: what cube-v2r1 showed
    setGLViewport(mainViewport());
    drawScene(viewMatrix(), projectionMatrix(), false);

    // Observer camera: the same scene seen from outside, plus the main camera and its frustum.
    // Only its own area is cleared, so the main view stays around the inset.
    if (scene.observerMode != ObserverMode::Off) {
        setGLViewport(observerViewport());
        glEnable(GL_SCISSOR_TEST);
        glClearColor(0.11f, 0.12f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDisable(GL_SCISSOR_TEST);

        drawScene(observerViewMatrix(), observerProjectionMatrix(), true);
    }

    paintLabels();

    ++frameCount;
}

void MainWidget::drawScene(const QMatrix4x4 &view, const QMatrix4x4 &projection, bool showMainCamera)
{
//! [6]
    // Both views draw the same world; only the camera (view and projection) is different
    const QMatrix4x4 viewProjection = projection * view;
    const QMatrix4x4 modelmatrix = modelMatrix();

    texture->bind();
    program.bind();

    // Set modelview-projection matrix
    program.setUniformValue("mvp_matrix", viewProjection * modelmatrix);
//! [6]

    // Use texture unit 0 which contains cube.png
    program.setUniformValue("cube_texture", 0);

    // Draw cube geometry. With the initial orientation the faces lie on x, z = +/-1, and so do some grid
    // lines; pulling the faces slightly towards the camera makes the face always win the depth test there
    // instead of flickering with the line (z-fighting)
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-1.0f, -1.0f);
    geometries->drawCubeGeometry(&program);
    glDisable(GL_POLYGON_OFFSET_FILL);

    // Reference lines in the world frame: the model matrix is the identity
    lineProgram.bind();
    if (scene.showGrid)
        drawLines(grid, viewProjection);
    if (scene.showWorldAxes)
        drawLines(worldAxes, viewProjection);

    // Point the main camera looks at
    QMatrix4x4 targetModel;
    targetModel.translate(scene.cameraTarget);
    targetModel.scale(kTargetMarkerSize);
    drawLines(targetMarker, viewProjection * targetModel);

    // The main camera as an object in the world. V maps world to camera coordinates,
    // so V^-1 places anything given in camera coordinates (eye at the origin, looking down -z) in the world.
    const QMatrix4x4 cameraToWorld = viewMatrix().inverted();
    if (showMainCamera) {
        // Eye to the corners of the near plane: the corners are (+/-w, +/-h, -near) in camera coordinates.
        // Nothing in this part is drawn by the main camera, because it is in front of the near plane.
        const float halfHeight = scene.nearPlane * std::tan(qDegreesToRadians(scene.fieldOfView) / 2.0f);
        QMatrix4x4 nearCorners = cameraToWorld;
        nearCorners.scale(halfHeight * aspectRatio(mainViewport()), halfHeight, scene.nearPlane);
        drawLines(viewPyramid, viewProjection * nearCorners);

        // View frustum: P x V maps the frustum to the NDC cube [-1, 1]^3, so (P x V)^-1 maps the cube back.
        // The result has w != 1; the GPU divides by w after this transform, like for any other vertex.
        drawLines(frustum, viewProjection * (projectionMatrix() * viewMatrix()).inverted());
    }

    // Frame axes are drawn last, without depth test, so the cube and coincident world axes never hide them
    glDisable(GL_DEPTH_TEST);
    if (scene.showObjectAxes) {
        QMatrix4x4 objectAxes = modelmatrix;    // object frame: Mo = To x Ro
        objectAxes.scale(kObjectAxesLength);
        drawLines(localAxes, viewProjection * objectAxes);
    }
    if (showMainCamera) {
        QMatrix4x4 cameraAxes = cameraToWorld;  // camera frame: V^-1
        cameraAxes.scale(kCameraAxesLength);
        drawLines(localAxes, viewProjection * cameraAxes);
    }
    glEnable(GL_DEPTH_TEST);
}

void MainWidget::drawLines(LineGeometry *lines, const QMatrix4x4 &mvp)
{
    lineProgram.setUniformValue("mvp_matrix", mvp);
    lines->draw(&lineProgram);
}

void MainWidget::paintLabels()
{
    if (scene.observerMode == ObserverMode::Off)
        return;

    // Text on top of the OpenGL image; QPainter can paint on a QOpenGLWidget inside paintGL()
    QPainter painter(this);
    const QRect main = mainViewport().adjusted(8, 6, -8, -6);
    const QRect observer = observerViewport();

    // The small inset only gets a title; the legend is also in the control panel
    const QString observerTitle = scene.observerMode == ObserverMode::Split
        ? tr("관찰자 뷰\n노랑: 시야 범위 (near~far)\n회색: 눈~near (잘리는 부분)")
        : tr("관찰자 뷰");

    painter.setPen(Qt::white);
    painter.drawText(main, Qt::AlignLeft | Qt::AlignTop, tr("메인 카메라"));
    painter.drawText(observer.adjusted(8, 6, -8, -6), Qt::AlignLeft | Qt::AlignTop, observerTitle);

    painter.setPen(QColor(150, 150, 150));
    painter.drawRect(observer.adjusted(0, 0, -1, -1));
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
    scene.cameraPosition = scene.cameraTarget + SceneState::orbitOffset(azimuth, elevation, distance);
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

void MainWidget::setProjection(float fieldOfView, float nearPlane, float farPlane)
{
    scene.fieldOfView = std::clamp(fieldOfView, kMinFieldOfView, kMaxFieldOfView);
    scene.nearPlane = std::clamp(nearPlane, kMinNear, kMaxNear);
    // The far plane always stays behind the near plane
    scene.farPlane = std::clamp(farPlane, scene.nearPlane + kMinDepthRange, kMaxFar);
    notifySceneChanged();
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

void MainWidget::setShowGrid(bool show)
{
    scene.showGrid = show;
    notifySceneChanged();
}

void MainWidget::setShowWorldAxes(bool show)
{
    scene.showWorldAxes = show;
    notifySceneChanged();
}

void MainWidget::setShowObjectAxes(bool show)
{
    scene.showObjectAxes = show;
    notifySceneChanged();
}

void MainWidget::setObserverMode(ObserverMode mode)
{
    scene.observerMode = mode;
    notifySceneChanged();
}

void MainWidget::setObserverOrbit(float azimuth, float elevation, float distance)
{
    // Same limits as the main camera orbit, with a larger distance range
    scene.observerAzimuth = std::remainder(azimuth, 360.0f);
    scene.observerElevation = std::clamp(elevation, -kMaxElevation, kMaxElevation);
    scene.observerDistance = std::clamp(distance, kMinObserverDistance, kMaxObserverDistance);
    notifySceneChanged();
}

void MainWidget::resetScene()
{
    // Poses (object, camera, observer) go back to the initial values;
    // the projection, auto rotation and display settings are kept
    initScene();
    notifySceneChanged();
}
