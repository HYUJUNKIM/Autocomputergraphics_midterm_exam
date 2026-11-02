#include "linegeometry.h"

// x red, y green, z blue
static const QVector3D kAxisColors[3] = {
    QVector3D(1.0f, 0.25f, 0.25f),
    QVector3D(0.3f, 0.9f, 0.3f),
    QVector3D(0.3f, 0.5f, 1.0f),
};

LineGeometry::LineGeometry(const QList<LineVertex> &vertices)
    : vertexCount(int(vertices.size()))
{
    if (!initializeOpenGLFunctions())
        qFatal("Failed to initialize OpenGL 3.3 Core line functions");

    // One VAO and one VBO; the segments are not indexed
    if (!vertexArrayObject.create())
        qFatal("Failed to create vertex array object");
    arrayBuf.create();

    // Transfer vertex data to the VBO
    arrayBuf.bind();
    arrayBuf.allocate(vertices.constData(), vertexCount * int(sizeof(LineVertex)));
}

LineGeometry::~LineGeometry()
{
    vertexArrayObject.destroy();
    arrayBuf.destroy();
}

void LineGeometry::draw(QOpenGLShaderProgram *program)
{
    QOpenGLVertexArrayObject::Binder vertexArrayBinder(&vertexArrayObject);

    arrayBuf.bind();

    // Offset for position
    quintptr offset = 0;

    int vertexLocation = program->attributeLocation("a_position");
    program->enableAttributeArray(vertexLocation);
    program->setAttributeBuffer(vertexLocation, GL_FLOAT, offset, 3, sizeof(LineVertex));

    // Offset for color
    offset += sizeof(QVector3D);

    int colorLocation = program->attributeLocation("a_color");
    program->enableAttributeArray(colorLocation);
    program->setAttributeBuffer(colorLocation, GL_FLOAT, offset, 3, sizeof(LineVertex));

    glDrawArrays(GL_LINES, 0, vertexCount);
}

QList<LineVertex> LineGeometry::grid(int halfCount, float spacing)
{
    // Every 5th line is brighter, so distances are easy to count
    const QVector3D minor(0.25f, 0.25f, 0.25f);
    const QVector3D major(0.45f, 0.45f, 0.45f);
    const float extent = halfCount * spacing;

    QList<LineVertex> vertices;
    for (int i = -halfCount; i <= halfCount; ++i) {
        // The center lines are left out: the world x and z axes are drawn there,
        // and two lines at the same place would flicker (z-fighting)
        if (i == 0)
            continue;

        const float c = i * spacing;
        const QVector3D color = (i % 5 == 0) ? major : minor;
        vertices << LineVertex{QVector3D(-extent, 0.0f, c), color}   // parallel to x
                 << LineVertex{QVector3D( extent, 0.0f, c), color}
                 << LineVertex{QVector3D(c, 0.0f, -extent), color}   // parallel to z
                 << LineVertex{QVector3D(c, 0.0f,  extent), color};
    }
    return vertices;
}

QList<LineVertex> LineGeometry::worldAxes(const QVector3D &extent)
{
    QList<LineVertex> vertices;
    for (int i = 0; i < 3; ++i) {
        QVector3D end;
        end[i] = extent[i];

        // Positive half in the full color, negative half dimmed
        const QVector3D dim = kAxisColors[i] * 0.4f;
        vertices << LineVertex{QVector3D(), kAxisColors[i]} << LineVertex{end, kAxisColors[i]}
                 << LineVertex{QVector3D(), dim} << LineVertex{-end, dim};
    }
    return vertices;
}

QList<LineVertex> LineGeometry::axes()
{
    QList<LineVertex> vertices;
    for (int i = 0; i < 3; ++i) {
        QVector3D end;
        end[i] = 1.0f;
        vertices << LineVertex{QVector3D(), kAxisColors[i]} << LineVertex{end, kAxisColors[i]};
    }
    return vertices;
}

QList<LineVertex> LineGeometry::box(const QVector3D &color)
{
    // Corner i has the coordinates -1 or +1 according to bits 0, 1 and 2 of i;
    // an edge joins two corners that differ in one bit
    auto corner = [](int i) {
        return QVector3D((i & 1) ? 1.0f : -1.0f, (i & 2) ? 1.0f : -1.0f, (i & 4) ? 1.0f : -1.0f);
    };

    QList<LineVertex> vertices;
    for (int i = 0; i < 8; ++i) {
        for (int bit = 1; bit <= 4; bit <<= 1) {
            if (!(i & bit))
                vertices << LineVertex{corner(i), color} << LineVertex{corner(i | bit), color};
        }
    }
    return vertices;
}

QList<LineVertex> LineGeometry::pyramid(const QVector3D &color)
{
    QList<LineVertex> vertices;
    for (float x : { -1.0f, 1.0f }) {
        for (float y : { -1.0f, 1.0f })
            vertices << LineVertex{QVector3D(), color} << LineVertex{QVector3D(x, y, -1.0f), color};
    }
    return vertices;
}

QList<LineVertex> LineGeometry::star(const QVector3D &color)
{
    // Diagonals instead of a +-shaped cross, so the marker never lies on a world axis
    QList<LineVertex> vertices;
    for (const QVector3D &d : { QVector3D(1, 1, 1), QVector3D(1, 1, -1), QVector3D(1, -1, 1), QVector3D(1, -1, -1) })
        vertices << LineVertex{-d, color} << LineVertex{d, color};
    return vertices;
}
