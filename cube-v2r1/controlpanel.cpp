#include "controlpanel.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>

#include <cmath>

// Avoid showing "-0.00" for values that are zero up to rounding
static double tidy(double value)
{
    return std::abs(value) < 0.005 ? 0.0 : value;
}

SliderSpinBox::SliderSpinBox(double minimum, double maximum, double step, int decimals,
                             const QString &suffix, QWidget *parent)
    : QWidget(parent)
    , slider(new QSlider(Qt::Horizontal, this))
    , spinBox(new QDoubleSpinBox(this))
    , scale(1.0 / step)
{
    slider->setRange(qRound(minimum * scale), qRound(maximum * scale));
    spinBox->setRange(minimum, maximum);
    spinBox->setSingleStep(step);
    spinBox->setDecimals(decimals);
    spinBox->setSuffix(suffix);
    spinBox->setKeyboardTracking(false);   // emit when Enter is pressed, not on every key
    spinBox->setMinimumWidth(82);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(slider, 1);
    layout->addWidget(spinBox);

    connect(slider, &QSlider::valueChanged, this, [this](int sliderValue) {
        const double newValue = sliderValue / scale;
        const QSignalBlocker blocker(spinBox);
        spinBox->setValue(newValue);
        emit valueChanged(newValue);
    });
    connect(spinBox, &QDoubleSpinBox::valueChanged, this, [this](double newValue) {
        const QSignalBlocker blocker(slider);
        slider->setValue(qRound(newValue * scale));
        emit valueChanged(newValue);
    });
}

double SliderSpinBox::value() const
{
    return spinBox->value();
}

void SliderSpinBox::setValue(double value)
{
    // Leave the widget alone while the user is dragging or typing in it
    if (slider->isSliderDown() || spinBox->hasFocus())
        return;

    const QSignalBlocker sliderBlocker(slider);
    const QSignalBlocker spinBoxBlocker(spinBox);
    slider->setValue(qRound(value * scale));
    spinBox->setValue(tidy(value));
}

Vector3Edit::Vector3Edit(double range, QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    const char *names[3] = { "X", "Y", "Z" };
    for (int i = 0; i < 3; ++i) {
        auto *spinBox = new QDoubleSpinBox(this);
        spinBox->setRange(-range, range);
        spinBox->setSingleStep(0.1);
        spinBox->setDecimals(2);
        spinBox->setKeyboardTracking(false);
        spinBox->setPrefix(QString::fromLatin1(names[i]) + QLatin1Char(' '));
        connect(spinBox, &QDoubleSpinBox::valueChanged, this, [this] { emit valueChanged(value()); });
        layout->addWidget(spinBox);
        spinBoxes[i] = spinBox;
    }
}

QVector3D Vector3Edit::value() const
{
    return QVector3D(spinBoxes[0]->value(), spinBoxes[1]->value(), spinBoxes[2]->value());
}

void Vector3Edit::setValue(const QVector3D &value)
{
    for (int i = 0; i < 3; ++i) {
        QDoubleSpinBox *spinBox = spinBoxes[i];
        if (spinBox->hasFocus())
            continue;
        const QSignalBlocker blocker(spinBox);
        spinBox->setValue(tidy(value[i]));
    }
}

ControlPanel::ControlPanel(QWidget *parent)
    : QWidget(parent)
{
    // Camera: position and target, or the same camera as orbit parameters around the target
    cameraPosition = new Vector3Edit(50.0, this);
    cameraTarget = new Vector3Edit(50.0, this);
    azimuth = new SliderSpinBox(-180.0, 180.0, 1.0, 0, tr("°"), this);
    elevation = new SliderSpinBox(-89.0, 89.0, 1.0, 0, tr("°"), this);
    distance = new SliderSpinBox(1.0, 50.0, 0.1, 1, QString(), this);
    auto *lookAtButton = new QPushButton(tr("물체 바라보기 (바라보는 점 = 물체 위치)"), this);

    auto *cameraBox = new QGroupBox(tr("카메라"), this);
    auto *cameraForm = new QFormLayout(cameraBox);
    cameraForm->addRow(tr("위치"), cameraPosition);
    cameraForm->addRow(tr("바라보는 점"), cameraTarget);
    cameraForm->addRow(tr("오빗 방위각"), azimuth);
    cameraForm->addRow(tr("오빗 고도각"), elevation);
    cameraForm->addRow(tr("거리"), distance);
    cameraForm->addRow(lookAtButton);

    // Object: position and orientation as Euler angles
    objectPosition = new Vector3Edit(50.0, this);
    pitch = new SliderSpinBox(-90.0, 90.0, 1.0, 0, tr("°"), this);
    yaw = new SliderSpinBox(-180.0, 180.0, 1.0, 0, tr("°"), this);
    roll = new SliderSpinBox(-180.0, 180.0, 1.0, 0, tr("°"), this);

    auto *objectBox = new QGroupBox(tr("물체"), this);
    auto *objectForm = new QFormLayout(objectBox);
    objectForm->addRow(tr("위치"), objectPosition);
    objectForm->addRow(tr("회전 X (pitch)"), pitch);
    objectForm->addRow(tr("회전 Y (yaw)"), yaw);
    objectForm->addRow(tr("회전 Z (roll)"), roll);

    // Auto rotation about the world y axis
    autoRotate = new QCheckBox(tr("켜기 (Space)"), this);
    autoRotateSpeed = new SliderSpinBox(0.0, 360.0, 1.0, 0, tr("°/s"), this);

    auto *autoBox = new QGroupBox(tr("자동 회전 (월드 y축 기준)"), this);
    auto *autoForm = new QFormLayout(autoBox);
    autoForm->addRow(autoRotate);
    autoForm->addRow(tr("속도"), autoRotateSpeed);

    auto *help = new QLabel(tr(
        "왼쪽 드래그: 물체 회전\n"
        "오른쪽 드래그 / Shift+왼쪽 드래그: 카메라 오빗\n"
        "방향키: 카메라 오빗\n"
        "휠: 확대/축소 (카메라와 바라보는 점 사이 거리)\n"
        "W/S: 앞/뒤, A/D: 왼쪽/오른쪽, Q/E: 아래/위 이동\n"
        "R: 초기화\n"
        "키보드는 3D 화면을 한 번 클릭한 뒤 동작합니다"), this);
    help->setWordWrap(true);
    auto *helpBox = new QGroupBox(tr("조작 방법"), this);
    auto *helpLayout = new QVBoxLayout(helpBox);
    helpLayout->addWidget(help);

    auto *resetButton = new QPushButton(tr("초기화 (R)"), this);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(cameraBox);
    layout->addWidget(objectBox);
    layout->addWidget(autoBox);
    layout->addWidget(helpBox);
    layout->addWidget(resetButton);
    layout->addStretch(1);

    // Object names identify the controls (also used by tests and style sheets)
    cameraPosition->setObjectName("cameraPosition");
    cameraTarget->setObjectName("cameraTarget");
    azimuth->setObjectName("azimuth");
    elevation->setObjectName("elevation");
    distance->setObjectName("distance");
    objectPosition->setObjectName("objectPosition");
    pitch->setObjectName("pitch");
    yaw->setObjectName("yaw");
    roll->setObjectName("roll");
    autoRotate->setObjectName("autoRotate");
    autoRotateSpeed->setObjectName("autoRotateSpeed");

    // Panel -> scene: every edit is forwarded as a signal, MainWindow connects them to MainWidget
    connect(cameraPosition, &Vector3Edit::valueChanged, this, &ControlPanel::cameraPositionEdited);
    connect(cameraTarget, &Vector3Edit::valueChanged, this, &ControlPanel::cameraTargetEdited);
    connect(azimuth, &SliderSpinBox::valueChanged, this, &ControlPanel::emitOrbit);
    connect(elevation, &SliderSpinBox::valueChanged, this, &ControlPanel::emitOrbit);
    connect(distance, &SliderSpinBox::valueChanged, this, &ControlPanel::emitOrbit);
    connect(lookAtButton, &QPushButton::clicked, this, &ControlPanel::lookAtObjectRequested);
    connect(objectPosition, &Vector3Edit::valueChanged, this, &ControlPanel::objectPositionEdited);
    connect(pitch, &SliderSpinBox::valueChanged, this, &ControlPanel::emitEulerAngles);
    connect(yaw, &SliderSpinBox::valueChanged, this, &ControlPanel::emitEulerAngles);
    connect(roll, &SliderSpinBox::valueChanged, this, &ControlPanel::emitEulerAngles);
    connect(autoRotate, &QCheckBox::toggled, this, &ControlPanel::autoRotateToggled);
    connect(autoRotateSpeed, &SliderSpinBox::valueChanged, this, [this](double speed) {
        emit autoRotateSpeedEdited(float(speed));
    });
    connect(resetButton, &QPushButton::clicked, this, &ControlPanel::resetRequested);
}

void ControlPanel::emitOrbit()
{
    emit cameraOrbitEdited(float(azimuth->value()), float(elevation->value()), float(distance->value()));
}

void ControlPanel::emitEulerAngles()
{
    emit objectEulerAnglesEdited(QVector3D(pitch->value(), yaw->value(), roll->value()));
}

// Scene -> panel: called after every scene change, so mouse and keyboard edits show up here too
void ControlPanel::showState(const SceneState &state)
{
    cameraPosition->setValue(state.cameraPosition);
    cameraTarget->setValue(state.cameraTarget);
    azimuth->setValue(state.cameraAzimuth());
    elevation->setValue(state.cameraElevation());
    distance->setValue(state.cameraDistance());

    objectPosition->setValue(state.objectPosition);
    const QVector3D euler = state.objectOrientation.toEulerAngles();
    pitch->setValue(euler.x());
    yaw->setValue(euler.y());
    roll->setValue(euler.z());

    {
        const QSignalBlocker blocker(autoRotate);
        autoRotate->setChecked(state.autoRotate);
    }
    autoRotateSpeed->setValue(state.autoRotateSpeed);
}
