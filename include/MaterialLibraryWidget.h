#pragma once
#include <QTreeWidget>
#include "GLMaterial.h"
#include <QList>
#include <QMap>
#include <QVector>
#include <QPair>

class MaterialLibraryWidget : public QTreeWidget
{
    Q_OBJECT
public:
    explicit MaterialLibraryWidget(QWidget *parent = nullptr);

    // call this once at app startup to populate the shared material map + groups
    static bool loadAllMaterials(const QString& jsonPath, QString* err = nullptr);

    // reload from registry (e.g. after user edits or imports) — convenience wrapper
    static bool reloadAllMaterials(QString* err = nullptr) { return loadAllMaterials(s_jsonPath, err); }

    // Merge user-defined materials from per-user JSON into the shared cache.
    // Returns true on success (or when no user file exists). On error returns false and sets err if provided.
    static bool mergeUserMaterialsFromUserLocation(QString* err = nullptr);


    // Accessors for read-only shared data (optional)
    static const QMap<QString, std::function<GLMaterial()>>& sharedMaterialMap() { return s_materialMap; }
    static const QVector<QPair<QString, QVector<QPair<QString, QString>>>>& sharedGroups() { return s_groups; }


signals:
    void materialSelected(const GLMaterial &mat);

private slots:
    void onItemClicked(QTreeWidgetItem *item, int column);
    void handleItemEntered(QTreeWidgetItem* item, int column);

private:
    void populateMaterials();
    static QVector<QPair<QString, QVector<QPair<QString, QString>>>> populateMaterialMapWithBuiltIns(
        QMap<QString, std::function<GLMaterial()>>& materialMap);

private:
    // static storage
    static QMap<QString, std::function<GLMaterial()>> s_materialMap;
    // Groups: vector of (groupLabel, vector of (displayName, key))
    static QVector<QPair<QString, QVector<QPair<QString, QString>>>> s_groups;

    // remember jsonPath used for reloads
    static QString s_jsonPath;
};
