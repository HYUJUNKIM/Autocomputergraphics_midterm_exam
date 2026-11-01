# cube-v2r1

cube-v2를 복사해서 Qt 기능을 추가한 버전입니다. cube-v2 원본은 그대로 두고 이 폴더에서만 수정합니다.

## 추가한 기능

| 기능 | 사용한 Qt 기능 | 코드 위치 |
|---|---|---|
| 상태바에 FPS, 카메라 위치와 거리, 물체 회전 각도 표시 | `QStatusBar`, `QLabel`, `QTimer`, `QElapsedTimer` | `mainwindow.cpp` |
| 옆 패널에서 카메라와 물체의 위치, 회전값을 슬라이더나 숫자로 조절 | `QDockWidget`, `QSlider`, `QDoubleSpinBox`, `QFormLayout` | `controlpanel.cpp` |
| 마우스나 키보드로 바꾼 값이 패널에도 반영되는 양방향 연동 | signal/slot, `QSignalBlocker` | `mainwindow.cpp`, `ControlPanel::showState()` |
| 휠로 확대/축소 (카메라와 바라보는 점 사이 거리) | `wheelEvent`, `QWheelEvent::angleDelta()` | `MainWidget::wheelEvent()` |
| 키보드(W A S D Q E)로 카메라 이동 | `keyPressEvent`, `setFocusPolicy(Qt::StrongFocus)` | `MainWidget::moveCamera()` |
| 오빗 카메라 (물체는 고정, 카메라가 주위를 돎) | 구면좌표(방위각, 고도각, 거리) + `QMatrix4x4::lookAt` | `MainWidget::setCameraOrbit()` |
| 자동 회전 켜기/끄기, 속도 조절 | `QTimer` + `QElapsedTimer` (경과 시간 기준) | `MainWidget::stepAutoRotation()` |

## 조작 방법

키보드는 3D 화면을 한 번 클릭한 뒤 동작합니다.

| 입력 | 동작 |
|---|---|
| 왼쪽 드래그 | 물체 회전 (cube-v2와 같음) |
| 오른쪽 드래그, Shift + 왼쪽 드래그, 방향키 | 카메라 오빗 |
| 휠 | 확대/축소 |
| W / S, A / D, Q / E | 카메라 앞/뒤, 왼쪽/오른쪽, 아래/위 이동 (카메라와 바라보는 점이 같이 이동) |
| Space | 자동 회전 켜기/끄기 |
| R | 초기화 (cube-v2 초기 장면으로) |

## 파일 구성

| 파일 | 상태 | 내용 |
|---|---|---|
| `scenestate.h` | 새 파일 | 장면 상태(물체 자세, 카메라, 자동 회전)를 하나로 묶은 구조체. 오빗 값(방위각, 고도각, 거리)을 계산하는 함수 포함 |
| `mainwidget.h/.cpp` | 수정 | 렌더링과 입력 처리. 장면이 바뀔 때마다 `sceneChanged()` signal을 보냄 |
| `controlpanel.h/.cpp` | 새 파일 | 옆 패널. 값을 고치면 `...Edited` signal을 보내고, `showState()`로 장면 값을 받아 표시 |
| `mainwindow.h/.cpp` | 새 파일 | 창 구성(가운데 3D 화면, 오른쪽 패널, 아래 상태바)과 signal/slot 연결 |
| `main.cpp` | 수정 | `MainWidget` 대신 `MainWindow`를 띄움 |
| `CMakeLists.txt`, `CMakePresets.json`, `cube.pro` | 수정 | 빌드 대상 이름을 `cube-v2r1`로 바꾸고 새 파일 추가. MSVC에 `/utf-8` 추가 (한글 UI 문자열) |
| `geometryengine.*`, 셰이더, `cube.png`, `*.qrc` | 그대로 | cube-v2와 같음 |

## cube-v2에서 달라진 동작

- **투영 범위**: near 3 / far 7 → near 0.1 / far 100. 카메라 거리가 1~50 사이로 바뀌기 때문입니다.
- **회전축 계산**: cube-v2는 마지막으로 그린 프레임의 행렬을 썼습니다. 이 버전은 현재 장면 상태로 바로 계산해서, 화면을 다시 그리기 전에 마우스 이벤트가 여러 번 와도 축이 어긋나지 않습니다.
- **장면 초기화 위치**: `initScene()`을 `initializeGL()`이 아니라 생성자에서 호출합니다. 장면 상태는 OpenGL과 관계가 없어서, 창이 뜨기 전에 패널이 값을 표시할 수 있습니다.
- **카메라 제한**: 고도각은 ±89°로 제한합니다. 위쪽 방향이 월드 y축이라 카메라가 정확히 위나 아래를 보면 `lookAt`을 계산할 수 없기 때문입니다.
- **FPS 표시**: 이 앱은 바뀐 것이 있을 때만 다시 그립니다. 그래서 아무것도 움직이지 않으면 FPS가 0으로 표시되고, 자동 회전 중에는 약 60이 나옵니다.

## 빌드

- **macOS**: `CMakeUserPresets.json`의 `macos` 프리셋을 씁니다(Apple Clang, Ninja, Homebrew Qt).
- **Windows**: `CMakePresets.json`의 `msvc2022-x64` 프리셋을 씁니다. Windows/MSVC 빌드는 직접 확인하지 않았습니다.
