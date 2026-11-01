// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#ifndef MAINWIDGET_H
#define MAINWIDGET_H

#include "geometryengine.h"
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
    void setAutoRotate(bool enabled);
    void setAutoRotateSpeed(float degreesPerSecond);
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
    QMatrix4x4 viewMatrix() const;

    QOpenGLShaderProgram program;
    GeometryEngine *geometries = nullptr;

    QOpenGLTexture *texture = nullptr;

    QMatrix4x4 projection;

    // Object pose, camera and auto rotation settings, initialized in initScene()
    SceneState scene;

    QVector2D lastMousePosition;

    // Auto rotation: the timer drives the animation, the clock measures the real elapsed time
    QTimer animationTimer;
    QElapsedTimer animationClock;

    int frameCount = 0;
};

#endif // MAINWIDGET_H
