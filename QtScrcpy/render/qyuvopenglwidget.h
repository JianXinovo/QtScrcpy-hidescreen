#ifndef QYUVOPENGLWIDGET_H
#define QYUVOPENGLWIDGET_H
#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLWidget>

class QYUVOpenGLWidget
    : public QOpenGLWidget
    , protected QOpenGLFunctions
{
    Q_OBJECT
public:
    explicit QYUVOpenGLWidget(QWidget *parent = nullptr);
    virtual ~QYUVOpenGLWidget() override;

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

    void setFrameSize(const QSize &frameSize);
    const QSize &frameSize();
    void updateTextures(quint8 *dataY, quint8 *dataU, quint8 *dataV, quint32 linesizeY, quint32 linesizeU, quint32 linesizeV);

    // 隐藏画面（留空）模式：不再上传/绘制视频纹理，只做一次清屏。
    // 控件本身既不 hide() 也不改几何，因此鼠标坐标换算、点击、拖拽全部保持原样。
    void setBlank(bool blank);
    bool isBlank() const;

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int width, int height) override;

private:
    void initShader();
    void initTextures();
    void deInitTextures();
    void updateTexture(GLuint texture, quint32 textureType, quint8 *pixels, quint32 stride);

private:
    // 视频帧尺寸
    QSize m_frameSize = { -1, -1 };
    bool m_needUpdate = false;
    bool m_textureInited = false;
    // 是否处于"隐藏画面"（只留空）模式
    bool m_blank = false;

    // 顶点缓冲对象(Vertex Buffer Objects, VBO)：默认即为VertexBuffer(GL_ARRAY_BUFFER)类型
    QOpenGLBuffer m_vbo;

    // 着色器程序：编译链接着色器
    QOpenGLShaderProgram m_shaderProgram;

    // YUV纹理，用于生成纹理贴图
    GLuint m_texture[3] = { 0 };
};

#endif // QYUVOPENGLWIDGET_H
