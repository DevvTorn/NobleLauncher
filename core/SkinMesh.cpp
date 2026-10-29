#include "SkinMesh.h"

#include <QVector3D>

namespace Noble {

namespace {

struct Face {
    QVector3D corners[4];
    QVector3D normal;
    float u0, v0, u1, v1;
};

struct PartSpec {
    int baseX = 0;
    int baseY = 0;
    int width = 8;
    int height = 8;
    int depth = 8;
};

PartSpec specFor(const QString &part, bool slim)
{
    if (part == QLatin1String("body"))
        return {16, 16, 8, 12, 4};
    if (part == QLatin1String("rightArm"))
        return {40, 16, slim ? 3 : 4, 12, 4};
    if (part == QLatin1String("leftArm"))
        return {32, 48, slim ? 3 : 4, 12, 4};
    if (part == QLatin1String("rightLeg"))
        return {0, 16, 4, 12, 4};
    if (part == QLatin1String("leftLeg"))
        return {16, 48, 4, 12, 4};
    return {0, 0, 8, 8, 8};
}

QVector<Face> facesFor(const PartSpec &spec, bool slim, bool isHead)
{
    Q_UNUSED(slim)
    const int w = spec.width;
    const int h = spec.height;
    const int d = spec.depth;
    const int bx = spec.baseX;
    const int by = spec.baseY;
    const float hw = w / 2.0f;
    const float hh = h / 2.0f;
    const float hd = d / 2.0f;
    const float U = 1.0f / 64.0f;
    auto rect = [&](int x, int y, int rw, int rh) {
        return QVector4D(x * U, y * U, (x + rw) * U, (y + rh) * U);
    };

    QVector<Face> faces;
    // front (+Z)
    {
        const auto r = rect(bx + d, by + d, w, h);
        faces.append({{{-hw, -hh, hd}, {hw, -hh, hd}, {hw, hh, hd}, {-hw, hh, hd}},
                      {0, 0, 1}, r.x(), r.y(), r.z(), r.w()});
    }
    // back (-Z)
    {
        const auto r = rect(bx + d + w + w, by + d, d, h);
        faces.append({{ {hw, -hh, -hd}, {-hw, -hh, -hd}, {-hw, hh, -hd}, {hw, hh, -hd} },
                      {0, 0, -1}, r.x(), r.y(), r.z(), r.w()});
    }
    // right (-X)
    {
        const auto r = rect(bx, by + d, d, h);
        faces.append({{ {-hw, -hh, -hd}, {-hw, -hh, hd}, {-hw, hh, hd}, {-hw, hh, -hd} },
                      {-1, 0, 0}, r.x(), r.y(), r.z(), r.w()});
    }
    // left (+X)
    {
        const auto r = rect(bx + d + w, by + d, d, h);
        faces.append({{ {hw, -hh, hd}, {hw, -hh, -hd}, {hw, hh, -hd}, {hw, hh, hd} },
                      {1, 0, 0}, r.x(), r.y(), r.z(), r.w()});
    }
    // top (+Y)
    {
        const auto r = rect(bx + d, by, w, d);
        faces.append({{ {-hw, hh, hd}, {hw, hh, hd}, {hw, hh, -hd}, {-hw, hh, -hd} },
                      {0, 1, 0}, r.x(), r.y(), r.z(), r.w()});
    }
    // bottom (-Y)
    {
        const auto r = rect(bx + d + w, by, w, d);
        faces.append({{ {-hw, -hh, -hd}, {hw, -hh, -hd}, {hw, -hh, hd}, {-hw, -hh, hd} },
                      {0, -1, 0}, r.x(), r.y(), r.z(), r.w()});
    }
    Q_UNUSED(isHead)
    return faces;
}

// UV координаты вершин: (0,0) — левый верхний угол текстуры (flipV у Texture).
struct UV { float u, v; };

UV uvFor(int face, int corner, const Face &f, bool bottomFace)
{
    Q_UNUSED(face)
    // Для боковых граней и верха: bl -> (u0,v1), br -> (u1,v1), tr -> (u1,v0), tl -> (u0,v0).
    if (bottomFace) {
        const UV table[4] = {{f.u0, f.v0}, {f.u1, f.v0}, {f.u1, f.v1}, {f.u0, f.v1}};
        return table[corner % 4];
    }
    const UV table[4] = {{f.u0, f.v1}, {f.u1, f.v1}, {f.u1, f.v0}, {f.u0, f.v0}};
    return table[corner % 4];
}

} // namespace

SkinMesh::SkinMesh(QQuick3DObject *parent)
    : QQuick3DGeometry(parent)
{
    rebuild();
}

void SkinMesh::setPart(const QString &part)
{
    if (m_part == part)
        return;
    m_part = part;
    emit partChanged();
    rebuild();
}

void SkinMesh::setSlim(bool slim)
{
    if (m_slim == slim)
        return;
    m_slim = slim;
    emit slimChanged();
    rebuild();
}

void SkinMesh::rebuild()
{
    clear();
    const PartSpec spec = specFor(m_part, m_slim);
    const QVector<Face> faces = facesFor(spec, m_slim, m_part == QLatin1String("head"));
    QByteArray vertexData;
    QByteArray indexData;
    const int stride = 8 * sizeof(float);
    QVector<quint32> indices;
    QVector3D minBound(1e9f, 1e9f, 1e9f);
    QVector3D maxBound(-1e9f, -1e9f, -1e9f);

    for (int faceIndex = 0; faceIndex < faces.size(); ++faceIndex) {
        const Face &face = faces.at(faceIndex);
        const bool bottom = face.normal.y() < 0;
        for (int corner = 0; corner < 4; ++corner) {
            const QVector3D position = face.corners[corner];
            const UV uv = uvFor(faceIndex, corner, face, bottom);
            float values[8] = {position.x(), position.y(), position.z(),
                               face.normal.x(), face.normal.y(), face.normal.z(),
                               uv.u, uv.v};
            vertexData.append(reinterpret_cast<const char *>(values), stride);
            minBound.setX(std::min(minBound.x(), position.x()));
            minBound.setY(std::min(minBound.y(), position.y()));
            minBound.setZ(std::min(minBound.z(), position.z()));
            maxBound.setX(std::max(maxBound.x(), position.x()));
            maxBound.setY(std::max(maxBound.y(), position.y()));
            maxBound.setZ(std::max(maxBound.z(), position.z()));
        }
        const int base = faceIndex * 4;
        indices << base + 0 << base + 1 << base + 2 << base + 0 << base + 2 << base + 3;
    }

    indexData.resize(indices.size() * static_cast<int>(sizeof(quint32)));
    memcpy(indexData.data(), indices.constData(), static_cast<size_t>(indexData.size()));

    setVertexData(vertexData);
    setIndexData(indexData);
    setStride(stride);
    setPrimitiveType(QQuick3DGeometry::PrimitiveType::Triangles);
    addAttribute(QQuick3DGeometry::Attribute::PositionSemantic, 0, QQuick3DGeometry::Attribute::ComponentType::F32Type);
    addAttribute(QQuick3DGeometry::Attribute::NormalSemantic, 3 * sizeof(float),
                 QQuick3DGeometry::Attribute::ComponentType::F32Type);
    addAttribute(QQuick3DGeometry::Attribute::TexCoord0Semantic, 6 * sizeof(float),
                 QQuick3DGeometry::Attribute::ComponentType::F32Type);
    addAttribute(QQuick3DGeometry::Attribute::IndexSemantic, 0, QQuick3DGeometry::Attribute::ComponentType::U32Type);
    setBounds(minBound, maxBound);
    update();
}

} // namespace Noble
