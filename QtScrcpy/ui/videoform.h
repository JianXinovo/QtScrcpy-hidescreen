#ifndef VIDEOFORM_H
#define VIDEOFORM_H

#include <QPointer>
#include <QTimer>
#include <QWidget>

#include "../QtScrcpyCore/include/QtScrcpyCore.h"

namespace Ui
{
    class videoForm;
}

class ToolForm;
class FileHandler;
class QYUVOpenGLWidget;
class KeyMapEditor;
class QLabel;
class MetalVideoWidget;
class VideoForm : public QWidget, public qsc::DeviceObserver
{
    Q_OBJECT
public:
    explicit VideoForm(bool framelessWindow = false, bool skin = true, bool showToolBar = true, int decodeMode = 0, QWidget *parent = 0);
    ~VideoForm();

    void staysOnTop(bool top = true);
    void updateShowSize(const QSize &newSize);
    void updateRender(int width, int height, uint8_t* dataY, uint8_t* dataU, uint8_t* dataV, int linesizeY, int linesizeU, int linesizeV);
    void setSerial(const QString& serial);
    QRect getGrabCursorRect();
    const QSize &frameSize();
    void resizeSquare();
    void removeBlackRect();
    void showFPS(bool show);
    void switchFullScreen();
    bool isHost();

    // 隐藏画面：不再显示手机画面（窗口只留外框，中间留空），
    // 但鼠标点击/拖拽/滚轮、键盘事件、自定义按键映射全部照常工作。
    void setScreenHidden(bool hidden);
    bool isScreenHidden() const;

    // 鼠标锁定：把指针钳制在画面区域内，再按一次解除。
    // 绑在 ` 键上（默认 keymap 的 switchKey 已让位给 F9）。
    void setCursorLocked(bool locked);
    bool isCursorLocked() const;

    // 键位可视化编辑（Ctrl+E 切换，Ctrl+S 保存并同步到设备）
    void setKeyMapFile(const QString &path);
    void toggleKeyMapEditor();
    // 用当前 keymap 的 switchKey 自动切到自定义映射（apply script 后调用）
    void activateCustomKeymap();

private:
    void onFrame(int width, int height, uint8_t* dataY, uint8_t* dataU, uint8_t* dataV,
                 int linesizeY, int linesizeU, int linesizeV) override;
    // VideoToolbox Metal 路径帧回调（仅 macOS arm64）
    void onFrameMetal(void* cvPixelBuffer, int width, int height) override;
    void updateFPS(quint32 fps) override;
    void onVideoSessionChanged(const QSize &size, bool clientResized) override;
    void grabCursor(bool grab) override;

    void updateStyleSheet(bool vertical);
    QMargins getMargins(bool vertical);
    void initUI();

    void showToolForm(bool show = true);
    void moveCenter();
    void installShortcut();
    QRect getScreenRect();
    // 窗口越界回收：显示器插拔后，若窗口落在任何屏幕的可用区域之外，
    // 将其移回主屏居中，避免成为看不见的"幽灵窗口"。
    void ensureOnScreen();
    // 是否与任一屏幕的可用区域有交集（含边界相交判定）
    bool isRectOnAnyScreen(const QRect &rect) const;

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void moveEvent(QMoveEvent *event) override;

    void paintEvent(QPaintEvent *) override;
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
#else
    bool nativeEvent(const QByteArray &eventType, void *message, long *result) override;
#endif

private:
    // 获取当前视频渲染 widget（OpenGL 或 Metal 容器）
    QWidget* videoWidget() const;
    // 是否使用 Metal 渲染路径
    bool isMetalMode() const;
    // 按当前窗口位置重新设置指针钳制矩形（窗口移动/缩放后必须刷新，否则锁定区域还是旧的）
    void updateCursorClip();
    // Windows 全局热键：不依赖窗口焦点，随时可切鼠标锁定
    void registerGlobalHotkeys();
    void unregisterGlobalHotkeys();

    // ui
    Ui::videoForm *ui;
    QPointer<ToolForm> m_toolForm;
    QPointer<QWidget> m_loadingWidget;
    QPointer<QYUVOpenGLWidget> m_videoWidget;

    // Metal 渲染路径（仅 macOS arm64）
    QPointer<MetalVideoWidget> m_metalWidget;

    QPointer<QLabel> m_fpsLabel;

    //inside member
    QSize m_frameSize;
    QSize m_normalSize;
    QPoint m_dragPosition;
    float m_widthHeightRatio = 0.5f;
    bool m_skin = true;
    QPoint m_fullScreenBeforePos;
    QString m_serial;
    int m_decodeMode = 0;
    bool m_metalFirstFrame = true;  // Metal 首次帧标记
    bool m_flexDisplay = false;
    bool m_preventAutoResize = false;
    // 画面隐藏（留空）状态
    bool m_screenHidden = false;
    // 隐藏画面时，留空区的左键按下先不派发给设备：
    // 抬起前位移没超过阈值算"点击"（补发一次触摸按下+抬起），
    // 超过阈值算"拖窗口"，这样既保住点击操作又能移动窗口。
    bool m_blankPressed = false;
    bool m_blankDragging = false;
    QPoint m_blankPressGlobal;
    QPoint m_blankDragOffset;
    // 鼠标是否被钳制在画面区域内
    bool m_cursorLocked = false;
    // 键位可视化编辑器（覆盖在画面上，默认隐藏）
    QPointer<KeyMapEditor> m_keymapEditor;
    // 鼠标锁定状态提示（短暂显示，用来确认到底锁没锁上）
    QPointer<QLabel> m_lockHint;
    QTimer m_flexResizeTimer;
    QSize m_pendingDisplaySize;

    //Whether to display the toolbar when connecting a device.
    bool show_toolbar = true;
};

#endif // VIDEOFORM_H
