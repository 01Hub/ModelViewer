#pragma once

#include "MaterialScanner.h"
#include <QMutex>

/**
 * MaterialTextureLibrary
 * ----------------------
 * Singleton that loads and caches all material texture presets once at startup.
 * Provides global access to the MaterialsMap.
 */
class MaterialTextureLibrary
{
public:
    static MaterialTextureLibrary& instance();

    // Non-copyable
    MaterialTextureLibrary(const MaterialTextureLibrary&) = delete;
    MaterialTextureLibrary& operator=(const MaterialTextureLibrary&) = delete;

    // Access the parsed materials
    const MaterialsMap& materials() const { return _materials; }

    // Reload from folder (optional)
    void reload(const QString& rootFolder);

private:
    MaterialTextureLibrary();   // private ctor
    ~MaterialTextureLibrary() = default;

    MaterialsMap _materials;
    mutable QMutex _mutex;
};
