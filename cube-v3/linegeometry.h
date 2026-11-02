#ifndef LINEGEOMETRY_H
#define LINEGEOMETRY_H

#include <QList>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QVector3D>

struct LineVertex
{
    QVector3D position;
    QVector3D color;
};

// Line segments with a color per vertex, drawn with GL_LINES.
// The vertices do not change after construction; the model matrix places them in the world.
class LineGeometry : protected QOpenGLFunctions_3_3_Core
{
public:
    // Every two vertices form one segment
    explicit LineGeometry(const QList<LineVertex> &vertices);
    virtual ~LineGeometry();

    void draw(QOpenGLShaderProgram *program);

    // Vertex lists for the reference lines drawn by MainWidget
    static QList<LineVertex> grid(int halfCount, float spacing);    // on the world x-z plane
    static QList<LineVertex> worldAxes(const QVector3D &extent);    // +/- half of each axis
    static QList<LineVertex> axes();                                // unit x, y, z axes from the origin
    static QList<LineVertex> box(const QVector3D &color);           // edges of the cube [-1, 1]^3
    static QList<LineVertex> pyramid(const QVector3D &color);       // origin to the corners (+/-1, +/-1, -1)
    static QList<LineVertex> star(const QVector3D &color);          // the 4 diagonals of the cube [-1, 1]^3

private:
    QOpenGLBuffer arrayBuf;
    QOpenGLVertexArrayObject vertexArrayObject;
    int vertexCount;
};

#endif // LINEGEOMETRY_H
