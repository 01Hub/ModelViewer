#pragma once
#include <QObject>
#include <QList>
#include <QMap>
#include <QVariantMap>
#include <QMutex>

class GLMaterial;

class MaterialRegistry : public QObject
{
    Q_OBJECT
public:
    struct Item
    {
        QString key;
        QString name;
        QVariantMap props; // raw properties read from JSON (variant map)
    };
    struct Group
    {
        QString id;
        QString label;
        QList<Item> items;
    };

    static MaterialRegistry& instance();

    // Load JSON file. Returns true on success; error message returned in err if provided.
    bool loadFromJsonFile(const QString& path, QString* err = nullptr);

    // Returns groups in the order loaded
    QList<Group> groups() const;

    // Returns a material instance for key (cached). If key not found returns an optional default material (constructed with defaults).
    GLMaterial materialForKey(const QString& key);

    // Returns whether registry has a key
    bool hasKey(const QString& key) const;

signals:
    // emitted after successful load (useful for UI to rebuild)
    void registryLoaded();

private:
    MaterialRegistry(QObject* parent = nullptr);
    ~MaterialRegistry() override = default;
    MaterialRegistry(const MaterialRegistry&) = delete;
    MaterialRegistry& operator=(const MaterialRegistry&) = delete;

    QList<Group> m_groups;
    QMap<QString, QVariantMap> m_rawByKey;
    mutable QMap<QString, QSharedPointer<GLMaterial>> m_cache;
    mutable QMutex m_cacheMutex;
};
