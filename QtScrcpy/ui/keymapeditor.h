#ifndef KEYMAPEDITOR_H
#define KEYMAPEDITOR_H

#include <QJsonObject>
#include <QPointF>
#include <QWidget>

class QJsonArray;

// 覆盖在手机画面之上的键位可视化编辑器。
//
// 数据源就是 keymap 目录里的那份 json，编辑器只改节点位置/按键，
// 不碰 QtScrcpyCore 的解析逻辑，因此生成的文件和手写格式完全一致。
class KeyMapEditor : public QWidget
{
    Q_OBJECT
public:
    explicit KeyMapEditor(QWidget *parent = nullptr);

    bool loadFile(const QString &path);
    bool saveFile();
    QString filePath() const;
    const QJsonObject &keyMap() const;

    void setEditMode(bool on);
    bool isEditMode() const;

signals:
    void keyMapChanged();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void paintEvent(QPaintEvent *paint) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    struct Handle
    {
        int node = -1;
        int corner = 0; // 0=pos/centerPos 1=startPos 2=endPos
    };

    QJsonArray nodes() const;
    void setNodes(const QJsonArray &arr);

    static QString typeOf(const QJsonObject &node);
    static QPointF relOf(const QJsonObject &node, int corner);
    static bool editableCorner(const QJsonObject &node, int corner);
    static QString labelOf(const QJsonObject &node);

    QPointF toWidget(const QPointF &rel) const;
    QPointF toRelative(const QPoint &p) const;
    Handle hitTest(const QPoint &p) const;
    void moveNode(int index, int corner, const QPointF &rel);

    void createNode(const QString &type, const QPoint &at);
    void removeNode(int index);
    void editNode(int index);
    void saveAs();

    QString m_path;
    QJsonObject m_root;
    int m_selected = -1;
    Handle m_drag;
    bool m_editing = false;
};

#endif // KEYMAPEDITOR_H
