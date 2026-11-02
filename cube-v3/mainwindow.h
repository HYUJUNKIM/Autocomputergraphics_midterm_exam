#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QElapsedTimer>
#include <QMainWindow>
#include <QTimer>

class ControlPanel;
class MainWidget;
class QLabel;

// Main window: the OpenGL view in the middle, the control panel in a dock on the right
// and the FPS / camera / rotation read-out in the status bar
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    void showScene();
    void updateFps();

    MainWidget *view;
    ControlPanel *panel;

    QLabel *fpsLabel;
    QLabel *cameraLabel;
    QLabel *rotationLabel;

    QTimer fpsTimer;
    QElapsedTimer fpsClock;
};

#endif // MAINWINDOW_H
