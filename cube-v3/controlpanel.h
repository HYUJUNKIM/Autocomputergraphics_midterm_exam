#ifndef CONTROLPANEL_H
#define CONTROLPANEL_H

#include "scenestate.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QSlider;

// A slider and a spin box that edit the same value
class SliderSpinBox : public QWidget
{
    Q_OBJECT

public:
    SliderSpinBox(double minimum, double maximum, double step, int decimals,
                  const QString &suffix, QWidget *parent = nullptr);

    double value() const;

    // Shows a value coming from the scene; does not emit valueChanged()
    void setValue(double value);

signals:
    void valueChanged(double value);

private:
    QSlider *slider;
    QDoubleSpinBox *spinBox;
    double scale;   // the slider works on integers: slider value = value * scale
};

// Three spin boxes for the x, y and z components of a vector
class Vector3Edit : public QWidget
{
    Q_OBJECT

public:
    explicit Vector3Edit(double range, QWidget *parent = nullptr);

    QVector3D value() const;

    // Shows a value coming from the scene; does not emit valueChanged()
    void setValue(const QVector3D &value);

signals:
    void valueChanged(const QVector3D &value);

private:
    QDoubleSpinBox *spinBoxes[3];
};

// Side panel: edits the scene and shows the scene state coming back from MainWidget
class ControlPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ControlPanel(QWidget *parent = nullptr);

public slots:
    void showState(const SceneState &state);

signals:
    void cameraPositionEdited(const QVector3D &position);
    void cameraTargetEdited(const QVector3D &target);
    void cameraOrbitEdited(float azimuth, float elevation, float distance);
    void lookAtObjectRequested();
    void projectionEdited(float fieldOfView, float nearPlane, float farPlane);
    void objectPositionEdited(const QVector3D &position);
    void objectEulerAnglesEdited(const QVector3D &pitchYawRoll);
    void autoRotateToggled(bool enabled);
    void autoRotateSpeedEdited(float degreesPerSecond);
    void showGridToggled(bool show);
    void showWorldAxesToggled(bool show);
    void showObjectAxesToggled(bool show);
    void observerModeEdited(ObserverMode mode);
    void observerOrbitEdited(float azimuth, float elevation, float distance);
    void resetRequested();

private:
    void emitOrbit();
    void emitProjection();
    void emitEulerAngles();
    void emitObserverOrbit();

    Vector3Edit *cameraPosition;
    Vector3Edit *cameraTarget;
    SliderSpinBox *azimuth;
    SliderSpinBox *elevation;
    SliderSpinBox *distance;
    SliderSpinBox *fieldOfView;
    SliderSpinBox *nearPlane;
    SliderSpinBox *farPlane;

    Vector3Edit *objectPosition;
    SliderSpinBox *pitch;
    SliderSpinBox *yaw;
    SliderSpinBox *roll;

    QCheckBox *autoRotate;
    SliderSpinBox *autoRotateSpeed;

    QCheckBox *showGrid;
    QCheckBox *showWorldAxes;
    QCheckBox *showObjectAxes;

    QComboBox *observerMode;
    SliderSpinBox *observerAzimuth;
    SliderSpinBox *observerElevation;
    SliderSpinBox *observerDistance;
};

#endif // CONTROLPANEL_H
