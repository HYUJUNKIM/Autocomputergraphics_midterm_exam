// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#ifndef MAINWIDGET_H
#define MAINWIDGET_H

#include "geometryengine.h"
#include "linegeometry.h"
#include "scenestate.h"

#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QMatrix4x4>
#include <QQuaternion>
#include <QVector2D>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QTimer>
#include <QElapsedTimer>

class GeometryEngine;

class MainWidget : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core
{
    Q_OBJECT

public:
    explicit MainWidget(QWidget *parent = nullptr);
    ~MainWidget();

    QSize sizeHint() const override;

    const SceneState &sceneState() const { return scene; }

    // Number of frames painted since the previous call (used for the FPS display)
    int takeFrameCount();

public slots:
    void setObjectPosition(const QVector3D &position);
    void setObjectEulerAngles(const QVector3D &pitchYawRoll);
    void setCameraPosition(const QVector3D &position);
    void setCameraTarget(const QVector3D &target);
    void setCameraOrbit(float azimuth, float elevation, float distance);
    void lookAtObject();
    void setProjection(float fieldOfView, float nearPlane, float farPlane);
    void setAutoRotate(bool enabled);
    void setAutoRotateSpeed(float degreesPerSecond);
    void setShowGrid(bool show);
    void setShowWorldAxes(bool show);
    void setShowObjectAxes(bool show);
    void setObserverMode(ObserverMode mode);
    void setObserverOrbit(float azimuth, float elevation, float distance);
    void resetScene();

signals:
    // Emitted whenever the scene state changes: mouse, wheel, keyboard, auto rotation or a slot call
    void sceneChanged();

protected:
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;

    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    void initShaders();
    void initTextures();

private:
    void initScene();
    void rotateObject(const QVector2D &diff);
    void orbitCamera(float deltaAzimuth, float deltaElevation);
    void moveCamera(float right, float up, float forward);
    void stepAutoRotation();
    void notifySceneChanged();

    // Transformations of the main camera, the object and the observer camera
    QMatrix4x4 modelMatrix() const;
    QMatrix4x4 viewMatrix() const;
    QMatrix4x4 projectionMatrix() const;
    QMatrix4x4 observerViewMatrix() const;
    QMatrix4x4 observerProjectionMatrix() const;

    // Areas of the widget (logical pixels, y down) where the two views are drawn
    QRect mainViewport() const;
    QRect observerViewport() const;
    void setGLViewport(const QRect &area);

    void drawScene(const QMatrix4x4 &view, const QMatrix4x4 &projection, bool showMainCamera);
    void drawLines(LineGeometry *lines, const QMatrix4x4 &mvp);
    void paintLabels();

    QOpenGLShaderProgram program;
    QOpenGLShaderProgram lineProgram;
    GeometryEngine *geometries = nullptr;

    // Reference lines; the vertices are fixed, the model matrix places them (see drawScene())
    LineGeometry *grid = nullptr;
    LineGeometry *worldAxes = nullptr;
    LineGeometry *localAxes = nullptr;       // object frame and camera frame
    LineGeometry *targetMarker = nullptr;
    LineGeometry *viewPyramid = nullptr;     // camera eye to the corners of the near plane
    LineGeometry *frustum = nullptr;         // NDC cube, mapped back to the world by (P x V)^-1

    QOpenGLTexture *texture = nullptr;

    // Object pose, camera and auto rotation settings, initialized in initScene()
    SceneState scene;

    QVector2D lastMousePosition;
    bool draggingObserver = false;   // the current drag started in the observer view

    // Auto rotation: the timer drives the animation, the clock measures the real elapsed time
    QTimer animationTimer;
    QElapsedTimer animationClock;

    int frameCount = 0;
};

#endif // MAINWIDGET_H
