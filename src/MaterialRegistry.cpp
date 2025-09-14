#include "MaterialRegistry.h"
#include "GLMaterial.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QDebug>

MaterialRegistry& MaterialRegistry::instance()
{
    static MaterialRegistry reg;
    return reg;
}

MaterialRegistry::MaterialRegistry(QObject* parent)
    : QObject(parent)
{
}

static QVariantMap jsonObjectToVariantMap(const QJsonObject& obj)
{
    QVariantMap map;
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it)
    {
        map.insert(it.key(), it.value().toVariant());
    }
    return map;
}

bool MaterialRegistry::loadFromJsonFile(const QString& path, QString* err)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
    {
        if (err) *err = QString("Failed to open %1: %2").arg(path, f.errorString());
        return false;
    }
    const QByteArray data = f.readAll();
    QJsonParseError jerr;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &jerr);
    if (jerr.error != QJsonParseError::NoError)
    {
        if (err) *err = QString("JSON parse error: %1").arg(jerr.errorString());
        return false;
    }
    if (!doc.isObject())
    {
        if (err) *err = QString("JSON root is not an object");
        return false;
    }

    QJsonObject root = doc.object();

    // Expect either "groups" (array of groups) or a flat "materials" map
    m_groups.clear();
    m_rawByKey.clear();

    if (root.contains(QStringLiteral("groups")) && root.value(QStringLiteral("groups")).isArray())
    {
        QJsonArray groupArray = root.value(QStringLiteral("groups")).toArray();
        for (const QJsonValue& gval : groupArray)
        {
            if (!gval.isObject()) continue;
            QJsonObject gobj = gval.toObject();
            Group grp;
            grp.id = gobj.value(QStringLiteral("id")).toString(gobj.value(QStringLiteral("label")).toString()).toLower().replace(' ', '_');
            grp.label = gobj.value(QStringLiteral("label")).toString(grp.id);
            if (gobj.contains(QStringLiteral("items")) && gobj.value(QStringLiteral("items")).isArray())
            {
                QJsonArray items = gobj.value(QStringLiteral("items")).toArray();
                for (const QJsonValue& itVal : items)
                {
                    if (!itVal.isObject()) continue;
                    QJsonObject itObj = itVal.toObject();
                    Item item;
                    item.key = itObj.value(QStringLiteral("key")).toString();
                    item.name = itObj.value(QStringLiteral("name")).toString(item.key);
                    // Save whole object as variant map (so GLMaterial::fromVariantMap can consume)
                    item.props = jsonObjectToVariantMap(itObj);
                    // remove name/key from props to avoid duplication (optional)
                    item.props.remove(QStringLiteral("key"));
                    item.props.remove(QStringLiteral("name"));
                    grp.items.append(item);

                    if (!item.key.isEmpty())
                    {
                        m_rawByKey.insert(item.key, item.props);
                    }
                }
            }
            m_groups.append(grp);
        }
    }
    else if (root.contains(QStringLiteral("materials")) && root.value(QStringLiteral("materials")).isObject())
    {
        // legacy flat map: materials: { KEY: { ... } }
        QJsonObject mats = root.value(QStringLiteral("materials")).toObject();
        Group all;
        all.id = "all";
        all.label = "All Materials";
        for (auto it = mats.constBegin(); it != mats.constEnd(); ++it)
        {
            Item item;
            item.key = it.key();
            item.name = it.key();
            if (it.value().isObject())
            {
                item.props = jsonObjectToVariantMap(it.value().toObject());
            }
            all.items.append(item);
            m_rawByKey.insert(item.key, item.props);
        }
        m_groups.append(all);
    }
    else
    {
        // unknown format
        if (err) *err = QString("JSON does not contain 'groups' or 'materials'");
        return false;
    }

    // Clear cache
    {
        QMutexLocker locker(&m_cacheMutex);
        m_cache.clear();
    }

    emit registryLoaded();
    return true;
}

QList<MaterialRegistry::Group> MaterialRegistry::groups() const
{
    return m_groups;
}

bool MaterialRegistry::hasKey(const QString& key) const
{
    return m_rawByKey.contains(key);
}

GLMaterial MaterialRegistry::materialForKey(const QString& key)
{
    // check cache
    {
        QMutexLocker locker(&m_cacheMutex);
        if (m_cache.contains(key))
        {
            QSharedPointer<GLMaterial> ptr = m_cache.value(key);
            if (!ptr.isNull()) return *ptr;
        }
    }

    // build from raw props
    if (!m_rawByKey.contains(key))
    {
        // Not found: return default material (call default ctor)
        return GLMaterial();
    }
    QVariantMap props = m_rawByKey.value(key);

    GLMaterial mat = GLMaterial::fromVariantMap(props);

    // cache
    {
        QMutexLocker locker(&m_cacheMutex);
        m_cache.insert(key, QSharedPointer<GLMaterial>(new GLMaterial(mat)));
    }

    return mat;
}
