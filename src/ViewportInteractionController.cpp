#include "ViewportInteractionController.h"

#include <QMatrix4x4>

CornerAxisPosition ViewportInteractionController::normalizeCornerAxisPosition(
    CornerAxisPosition position)
{
    switch (position)
    {
    case CornerAxisPosition::TOP_LEFT:
    case CornerAxisPosition::TOP_RIGHT:
    case CornerAxisPosition::BOTTOM_LEFT:
    case CornerAxisPosition::BOTTOM_RIGHT:
        return position;
    default:
        return CornerAxisPosition::TOP_RIGHT;
    }
}

bool ViewportInteractionController::convertPixelToRay(
    const QPoint& pixel, const QRect& viewport, int widgetHeight,
    const QMatrix4x4& view, const QMatrix4x4& projection,
    QVector3D& orig, QVector3D& dir)
{
    if (viewport.width() <= 0 || viewport.height() <= 0)
    {
        orig = QVector3D(0, 0, 0);
        dir  = QVector3D(0, 0, 0);
        return false;
    }

    const int   yInverted = widgetHeight - pixel.y() - 1;
    const float ndcX = (2.0f * (pixel.x() - viewport.x())) / viewport.width()  - 1.0f;
    const float ndcY = (2.0f * (yInverted  - viewport.y())) / viewport.height() - 1.0f;

    const QVector4D nearNdc(ndcX, ndcY, -1.0f, 1.0f);
    const QVector4D farNdc (ndcX, ndcY,  1.0f, 1.0f);
    const QMatrix4x4 inv = (projection * view).inverted();

    QVector4D nearWorld = inv * nearNdc;
    QVector4D farWorld  = inv * farNdc;
    if (qFuzzyIsNull(nearWorld.w()) || qFuzzyIsNull(farWorld.w()))
    {
        orig = QVector3D(0, 0, 0);
        dir  = QVector3D(0, 0, 0);
        return false;
    }

    nearWorld /= nearWorld.w();
    farWorld  /= farWorld.w();
    orig = nearWorld.toVector3D();
    const QVector3D rawDir = farWorld.toVector3D() - orig;
    dir = rawDir.isNull() ? QVector3D(0, 0, 0) : rawDir.normalized();
    return !dir.isNull();
}

bool ViewportInteractionController::intersectRayPlane(
    const QVector3D& rayOrigin, const QVector3D& rayDir,
    const QVector3D& planePoint, const QVector3D& planeNormal,
    QVector3D& outPoint)
{
    const float denom = QVector3D::dotProduct(rayDir, planeNormal);
    if (std::abs(denom) <= 1.0e-6f)
        return false;

    const float t = QVector3D::dotProduct(planePoint - rayOrigin, planeNormal) / denom;
    if (t < 0.0f)
        return false;

    outPoint = rayOrigin + rayDir * t;
    return true;
}

QVector3D ViewportInteractionController::rotatePointAroundAxis(
    const QVector3D& point, const QVector3D& pivot,
    const QVector3D& axis, float angleDegrees)
{
    QMatrix4x4 rot;
    rot.setToIdentity();
    rot.translate(pivot);
    rot.rotate(angleDegrees, axis);
    rot.translate(-pivot);
    return rot.map(point);
}
