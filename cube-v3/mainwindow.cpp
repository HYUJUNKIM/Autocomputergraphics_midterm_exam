#include "mainwindow.h"

#include "controlpanel.h"
#include "mainwidget.h"

#include <QDockWidget>
#include <QLabel>
#include <QMenuBar>
#include <QScrollArea>
#include <QStatusBar>

#include <cmath>

// Avoid showing "-0.00" for values that are zero up to rounding
static double tidy(double value)
{
    return std::abs(value) < 0.005 ? 0.0 : value;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , view(new MainWidget(this))
    , panel(new ControlPanel)
    , fpsLabel(new QLabel(this))
    , cameraLabel(new QLabel(this))
    , rotationLabel(new QLabel(this))
{
    setCentralWidget(view);

    // Control panel in a dock on the right; the scroll area keeps it usable in a small window
    auto *scrollArea = new QScrollArea;
    scrollArea->setWidget(panel);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto *dock = new QDockWidget(tr("장면 조절"), this);
    dock->setWidget(scrollArea);
    addDockWidget(Qt::RightDockWidgetArea, dock);
    menuBar()->addMenu(tr("보기"))->addAction(dock->toggleViewAction());

    // Tall enough for most of the panel; the view gets the rest of the width
    resize(1040, 720);

    // Room for the longest texts, so the labels do not overlap when the numbers get longer
    fpsLabel->setMinimumWidth(fontMetrics().horizontalAdvance(tr("FPS 000.0")) + 16);
    cameraLabel->setMinimumWidth(fontMetrics().horizontalAdvance(tr("카메라 (-00.00, -00.00, -00.00)  거리 00.00")) + 16);
    statusBar()->addWidget(fpsLabel);
    statusBar()->addWidget(cameraLabel);
    statusBar()->addWidget(rotationLabel, 1);

    // Panel -> scene
    connect(panel, &ControlPanel::cameraPositionEdited, view, &MainWidget::setCameraPosition);
    connect(panel, &ControlPanel::cameraTargetEdited, view, &MainWidget::setCameraTarget);
    connect(panel, &ControlPanel::cameraOrbitEdited, view, &MainWidget::setCameraOrbit);
    connect(panel, &ControlPanel::lookAtObjectRequested, view, &MainWidget::lookAtObject);
    connect(panel, &ControlPanel::projectionEdited, view, &MainWidget::setProjection);
    connect(panel, &ControlPanel::objectPositionEdited, view, &MainWidget::setObjectPosition);
    connect(panel, &ControlPanel::objectEulerAnglesEdited, view, &MainWidget::setObjectEulerAngles);
    connect(panel, &ControlPanel::autoRotateToggled, view, &MainWidget::setAutoRotate);
    connect(panel, &ControlPanel::autoRotateSpeedEdited, view, &MainWidget::setAutoRotateSpeed);
    connect(panel, &ControlPanel::observerModeEdited, view, &MainWidget::setObserverMode);
    connect(panel, &ControlPanel::observerOrbitEdited, view, &MainWidget::setObserverOrbit);
    connect(panel, &ControlPanel::showGridToggled, view, &MainWidget::setShowGrid);
    connect(panel, &ControlPanel::showWorldAxesToggled, view, &MainWidget::setShowWorldAxes);
    connect(panel, &ControlPanel::showObjectAxesToggled, view, &MainWidget::setShowObjectAxes);
    connect(panel, &ControlPanel::resetRequested, view, &MainWidget::resetScene);

    // Scene -> panel and status bar. Together with the connections above this is the two-way binding:
    // a mouse drag in the view changes the scene, and sceneChanged() moves the sliders.
    connect(view, &MainWidget::sceneChanged, this, &MainWindow::showScene);

    // FPS = frames painted during each 0.5 s window. Without auto rotation the view only repaints
    // when something changes, so the value drops to 0 while nothing moves.
    connect(&fpsTimer, &QTimer::timeout, this, &MainWindow::updateFps);
    fpsClock.start();
    fpsTimer.start(500);

    showScene();
    updateFps();
    view->setFocus();
}

void MainWindow::showScene()
{
    const SceneState &state = view->sceneState();
    panel->showState(state);

    const QVector3D camera = state.cameraPosition;
    cameraLabel->setText(tr("카메라 (%1, %2, %3)  거리 %4")
                             .arg(tidy(camera.x()), 0, 'f', 2)
                             .arg(tidy(camera.y()), 0, 'f', 2)
                             .arg(tidy(camera.z()), 0, 'f', 2)
                             .arg(state.cameraDistance(), 0, 'f', 2));

    const QVector3D euler = state.objectOrientation.toEulerAngles();
    rotationLabel->setText(tr("물체 회전 X %1°  Y %2°  Z %3°")
                               .arg(tidy(euler.x()), 0, 'f', 1)
                               .arg(tidy(euler.y()), 0, 'f', 1)
                               .arg(tidy(euler.z()), 0, 'f', 1));
}

void MainWindow::updateFps()
{
    const double seconds = fpsClock.restart() / 1000.0;
    const int frames = view->takeFrameCount();
    fpsLabel->setText(tr("FPS %1").arg(seconds > 0.0 ? frames / seconds : 0.0, 0, 'f', 1));
}
