#include <QCheckBox>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLineEdit>
#include <QLineF>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPair>
#include <QPushButton>
#include <QShortcut>

#include "keymapeditor.h"

namespace {

// Qt 键枚举 -> keymap 里的 "Key_xxx" 写法（QKeySequence 输出 "Space"/"A"/"F9"）
QString qtKeyName(int key)
{
    const QString text = QKeySequence(key).toString();
    if (text.isEmpty() || text.contains('+') || text.contains(',')) {
        return QString();
    }
    return "Key_" + text;
}

QJsonObject posObject(const QPointF &rel)
{
    QJsonObject obj;
    obj["x"] = rel.x();
    obj["y"] = rel.y();
    return obj;
}

QPointF objectPos(const QJsonValue &value)
{
    const QJsonObject obj = value.toObject();
    return QPointF(obj.value("x").toDouble(), obj.value("y").toDouble());
}

bool isMouseKey(const QString &key)
{
    return key == "LeftButton" || key == "RightButton" || key == "MiddleButton";
}

// 单个节点的属性编辑：鼠标键走下拉，键盘键"点一下然后按键盘"直接捕获。
class NodeEditor : public QDialog
{
public:
    NodeEditor(QWidget *parent, const QString &key, bool hasSwitchMap, bool switchMap)
        : QDialog(parent)
    {
        setWindowTitle(tr("Key binding"));
        auto *form = new QFormLayout(this);

        m_mouseBox = new QComboBox(this);
        m_mouseBox->addItem(tr("(keyboard)"), QString());
        m_mouseBox->addItem("LeftButton", "LeftButton");
        m_mouseBox->addItem("RightButton", "RightButton");
        m_mouseBox->addItem("MiddleButton", "MiddleButton");
        form->addRow(tr("Mouse button"), m_mouseBox);

        m_keyEdit = new QLineEdit(this);
        m_keyEdit->setPlaceholderText(isMouseKey(key) ? tr("press a key...") : key);
        m_keyEdit->setText(isMouseKey(key) ? QString() : key);
        m_keyEdit->installEventFilter(this);
        form->addRow(tr("Keyboard key"), m_keyEdit);

        if (hasSwitchMap) {
            m_switchMap = new QCheckBox(tr("release mouse (switchMap)"), this);
            m_switchMap->setChecked(switchMap);
            form->addRow(m_switchMap);
        }

        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        form->addRow(buttons);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

        if (isMouseKey(key)) {
            m_mouseBox->setCurrentIndex(qMax(1, m_mouseBox->findData(key)));
        }
    }

    QString key() const
    {
        const QString mouse = m_mouseBox->currentData().toString();
        if (!mouse.isEmpty()) {
            return mouse;
        }
        return m_keyEdit->text().trimmed();
    }

    bool switchMap() const
    {
        return m_switchMap ? m_switchMap->isChecked() : false;
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == m_keyEdit && event->type() == QEvent::KeyPress) {
            auto *keyEvent = static_cast<QKeyEvent *>(event);
            const QString name = qtKeyName(keyEvent->key());
            if (!name.isEmpty()) {
                m_mouseBox->setCurrentIndex(0);
                m_keyEdit->setText(name);
                return true;
            }
        }
        return QDialog::eventFilter(watched, event);
    }

private:
    QComboBox *m_mouseBox = nullptr;
    QLineEdit *m_keyEdit = nullptr;
    QCheckBox *m_switchMap = nullptr;
};

} // namespace

KeyMapEditor::KeyMapEditor(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    hide();
}

QJsonArray KeyMapEditor::nodes() const
{
    return m_root.value("keyMapNodes").toArray();
}

void KeyMapEditor::setNodes(const QJsonArray &arr)
{
    m_root["keyMapNodes"] = arr;
}

bool KeyMapEditor::loadFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError error {};
    const QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }
    m_root = doc.object();
    m_path = path;
    m_selected = -1;
    update();
    return true;
}

bool KeyMapEditor::saveFile()
{
    if (m_path.isEmpty()) {
        return false;
    }
    QFile file(m_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    file.write(QJsonDocument(m_root).toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

QString KeyMapEditor::filePath() const
{
    return m_path;
}

const QJsonObject &KeyMapEditor::keyMap() const
{
    return m_root;
}

void KeyMapEditor::setEditMode(bool on)
{
    m_editing = on;
    m_drag = Handle();
    setVisible(on);
    if (on) {
        raise();
    }
    update();
}

bool KeyMapEditor::isEditMode() const
{
    return m_editing;
}

bool KeyMapEditor::eventFilter(QObject *watched, QEvent *event)
{
    // 画面尺寸变化时（旋转、缩放）跟着铺满
    if (watched == parentWidget() && event->type() == QEvent::Resize) {
        setGeometry(parentWidget()->rect());
    }
    return QWidget::eventFilter(watched, event);
}

QString KeyMapEditor::typeOf(const QJsonObject &node)
{
    return node.value("type").toString();
}

bool KeyMapEditor::editableCorner(const QJsonObject &node, int corner)
{
    const QString type = typeOf(node);
    if (type == "KMT_STEER_WHEEL") {
        return corner == 0;
    }
    if (type == "KMT_DRAG") {
        return corner == 1 || corner == 2;
    }
    return corner == 0;
}

QPointF KeyMapEditor::relOf(const QJsonObject &node, int corner)
{
    const QString type = typeOf(node);
    if (type == "KMT_STEER_WHEEL") {
        return objectPos(node.value("centerPos"));
    }
    if (type == "KMT_DRAG") {
        return objectPos(corner == 2 ? node.value("endPos") : node.value("startPos"));
    }
    if (type == "KMT_CLICK_MULTI") {
        const QJsonArray clicks = node.value("clickNodes").toArray();
        if (!clicks.isEmpty()) {
            return objectPos(clicks.at(0).toObject().value("pos"));
        }
        return QPointF();
    }
    return objectPos(node.value("pos"));
}

QString KeyMapEditor::labelOf(const QJsonObject &node)
{
    const QString type = typeOf(node);
    if (type == "KMT_STEER_WHEEL") {
        return QString("%1/%2/%3/%4")
            .arg(node.value("upKey").toString(), node.value("leftKey").toString(),
                 node.value("downKey").toString(), node.value("rightKey").toString());
    }
    QString key = node.value("key").toString();
    if (key.isEmpty()) {
        key = node.value("comment").toString();
    }
    return key;
}

QPointF KeyMapEditor::toWidget(const QPointF &rel) const
{
    return QPointF(rel.x() * width(), rel.y() * height());
}

QPointF KeyMapEditor::toRelative(const QPoint &p) const
{
    if (width() <= 0 || height() <= 0) {
        return QPointF();
    }
    return QPointF(qBound(0.0, static_cast<double>(p.x()) / width(), 1.0),
                   qBound(0.0, static_cast<double>(p.y()) / height(), 1.0));
}

KeyMapEditor::Handle KeyMapEditor::hitTest(const QPoint &p) const
{
    const QJsonArray arr = nodes();
    Handle best;
    double bestDistance = 18.0;
    for (int i = 0; i < arr.size(); ++i) {
        const QJsonObject node = arr.at(i).toObject();
        for (int corner = 0; corner <= 2; ++corner) {
            if (!editableCorner(node, corner)) {
                continue;
            }
            const QPointF widgetPos = toWidget(relOf(node, corner));
            const double distance = QLineF(widgetPos, QPointF(p)).length();
            if (distance < bestDistance) {
                bestDistance = distance;
                best.node = i;
                best.corner = corner;
            }
        }
    }
    return best;
}

void KeyMapEditor::moveNode(int index, int corner, const QPointF &rel)
{
    QJsonArray arr = nodes();
    if (index < 0 || index >= arr.size()) {
        return;
    }
    QJsonObject node = arr.at(index).toObject();
    const QString type = typeOf(node);
    if (type == "KMT_STEER_WHEEL") {
        node["centerPos"] = posObject(rel);
    } else if (type == "KMT_DRAG") {
        node[corner == 2 ? "endPos" : "startPos"] = posObject(rel);
    } else if (type == "KMT_CLICK_MULTI") {
        // 多点连点：整体平移，保持各点的相对关系
        QJsonArray clicks = node.value("clickNodes").toArray();
        const QPointF delta = rel - relOf(node, 0);
        for (int i = 0; i < clicks.size(); ++i) {
            QJsonObject click = clicks.at(i).toObject();
            QPointF pos = objectPos(click.value("pos"));
            click["pos"] = posObject(QPointF(qBound(0.0, pos.x() + delta.x(), 1.0),
                                             qBound(0.0, pos.y() + delta.y(), 1.0)));
            clicks.replace(i, click);
        }
        node["clickNodes"] = clicks;
    } else {
        node["pos"] = posObject(rel);
    }
    arr.replace(index, node);
    setNodes(arr);
    update();
    emit keyMapChanged();
}

void KeyMapEditor::createNode(const QString &type, const QPoint &at)
{
    const QPointF rel = toRelative(at);
    QJsonObject node;
    node["type"] = type;
    node["comment"] = QString();

    if (type == "KMT_STEER_WHEEL") {
        node["centerPos"] = posObject(rel);
        node["leftOffset"] = 0.10;
        node["rightOffset"] = 0.10;
        node["upOffset"] = 0.15;
        node["downOffset"] = 0.15;
        node["leftKey"] = "Key_A";
        node["rightKey"] = "Key_D";
        node["upKey"] = "Key_W";
        node["downKey"] = "Key_S";
    } else if (type == "KMT_DRAG") {
        node["startPos"] = posObject(rel);
        node["endPos"] = posObject(QPointF(qMin(1.0, rel.x() + 0.2), rel.y()));
        node["key"] = "Key_F5";
        node["dragSpeed"] = 1.0;
        node["startDelay"] = 0;
    } else if (type == "KMT_CLICK_MULTI") {
        QJsonObject click;
        click["delay"] = 100;
        click["pos"] = posObject(rel);
        QJsonArray clicks;
        clicks.append(click);
        node["clickNodes"] = clicks;
        node["key"] = "Key_F7";
    } else {
        node["pos"] = posObject(rel);
        node["key"] = "Key_Space";
        if (type == "KMT_CLICK") {
            node["switchMap"] = false;
        }
    }

    QJsonArray arr = nodes();
    arr.append(node);
    setNodes(arr);
    m_selected = arr.size() - 1;
    update();
    emit keyMapChanged();

    // 新建后立刻让用户按键绑定
    editNode(m_selected);
}

void KeyMapEditor::removeNode(int index)
{
    QJsonArray arr = nodes();
    if (index < 0 || index >= arr.size()) {
        return;
    }
    arr.removeAt(index);
    setNodes(arr);
    m_selected = -1;
    update();
    emit keyMapChanged();
}

void KeyMapEditor::editNode(int index)
{
    QJsonArray arr = nodes();
    if (index < 0 || index >= arr.size()) {
        return;
    }
    QJsonObject node = arr.at(index).toObject();
    const QString type = typeOf(node);

    if (type == "KMT_STEER_WHEEL") {
        QMessageBox::information(this, tr("Steer wheel"),
                                 tr("Wheel center and offsets are edited by dragging the center point. "
                                    "Its four keys are stored as up/left/down/right in the key map page."));
        return;
    }

    const bool hasSwitchMap = (type == "KMT_CLICK");
    NodeEditor editor(this, node.value("key").toString(), hasSwitchMap, node.value("switchMap").toBool());
    if (editor.exec() != QDialog::Accepted) {
        return;
    }
    node["key"] = editor.key();
    if (hasSwitchMap) {
        node["switchMap"] = editor.switchMap();
    }
    arr.replace(index, node);
    setNodes(arr);
    update();
    emit keyMapChanged();
}

void KeyMapEditor::saveAs()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("Save key map as"), m_path, "*.json");
    if (path.isEmpty()) {
        return;
    }
    m_path = path;
    saveFile();
}

void KeyMapEditor::paintEvent(QPaintEvent *paint)
{
    Q_UNUSED(paint)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    painter.fillRect(rect(), QColor(12, 12, 16, 190));

    // 10% 网格，方便目测坐标
    painter.setPen(QPen(QColor(255, 255, 255, 26), 1));
    for (int i = 1; i < 10; ++i) {
        painter.drawLine(width() * i / 10, 0, width() * i / 10, height());
        painter.drawLine(0, height() * i / 10, width(), height() * i / 10);
    }

    const QJsonArray arr = nodes();
    for (int i = 0; i < arr.size(); ++i) {
        const QJsonObject node = arr.at(i).toObject();
        const QString type = typeOf(node);
        const bool selected = (i == m_selected);
        const QColor color = selected ? QColor(255, 196, 25) : QColor(0, 200, 255);

        painter.setPen(QPen(color, selected ? 2 : 1));

        if (type == "KMT_DRAG") {
            const QPointF from = toWidget(relOf(node, 1));
            const QPointF to = toWidget(relOf(node, 2));
            painter.drawLine(from, to);
            painter.setBrush(color);
            painter.drawEllipse(from, 6, 6);
            painter.drawEllipse(to, 6, 6);
            painter.drawText(from + QPointF(10, -8), labelOf(node));
            continue;
        }

        const QPointF center = toWidget(relOf(node, 0));
        if (type == "KMT_STEER_WHEEL") {
            const int radius = qMax(28, qMin(width(), height()) / 6);
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(center, radius, radius);
            painter.drawLine(center + QPointF(-radius, 0), center + QPointF(radius, 0));
            painter.drawLine(center + QPointF(0, -radius), center + QPointF(0, radius));
            painter.drawText(center + QPointF(radius + 6, 0), labelOf(node));
        } else {
            painter.setBrush(QColor(color.red(), color.green(), color.blue(), 60));
            painter.drawEllipse(center, 14, 14);
            painter.drawText(center + QPointF(18, 4), labelOf(node));
        }
    }
}

void KeyMapEditor::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }
    m_drag = hitTest(event->pos());
    m_selected = m_drag.node;
    if (m_drag.node >= 0) {
        event->accept();
    } else {
        // 点空白：建一个普通单击节点
        createNode("KMT_CLICK", event->pos());
        m_drag = Handle();
    }
    update();
}

void KeyMapEditor::mouseMoveEvent(QMouseEvent *event)
{
    if (m_drag.node < 0) {
        return;
    }
    moveNode(m_drag.node, m_drag.corner, toRelative(event->pos()));
    event->accept();
}

void KeyMapEditor::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event)
    m_drag = Handle();
}

void KeyMapEditor::mouseDoubleClickEvent(QMouseEvent *event)
{
    const Handle handle = hitTest(event->pos());
    if (handle.node >= 0) {
        editNode(handle.node);
    }
}

void KeyMapEditor::contextMenuEvent(QContextMenuEvent *event)
{
    const Handle handle = hitTest(event->pos());
    QMenu menu(this);

    if (handle.node >= 0) {
        menu.addAction(tr("Edit key binding"), this, [this, handle]() { editNode(handle.node); });
        menu.addAction(tr("Delete node"), this, [this, handle]() { removeNode(handle.node); });
    } else {
        QMenu *addMenu = menu.addMenu(tr("Add node"));
        const QList<QPair<QString, QString>> types {
            { "KMT_CLICK", tr("Click") },
            { "KMT_CLICK_TWICE", tr("Click twice") },
            { "KMT_CLICK_MULTI", tr("Click multi") },
            { "KMT_STEER_WHEEL", tr("Steer wheel (WASD)") },
            { "KMT_DRAG", tr("Drag") }
        };
        for (const auto &entry : types) {
            const QString type = entry.first;
            addMenu->addAction(entry.second, this, [this, type, event]() { createNode(type, event->pos()); });
        }
    }

    menu.addSeparator();
    menu.addAction(tr("Save"), this, [this]() { saveFile(); emit keyMapChanged(); });
    menu.addAction(tr("Save as..."), this, [this]() { saveAs(); });

    menu.exec(event->globalPos());
}
