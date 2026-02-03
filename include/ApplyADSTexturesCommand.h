#pragma once

#include "ModelViewerCommand.h"
#include <QMap>
#include <QSet>
#include <QUuid>
#include <QString>

class ApplyADSTexturesCommand : public ModelViewerCommand
{
public:
    ApplyADSTexturesCommand(ModelViewer* viewer,
        GLWidget* glWidget,
        const QVector<QUuid>& meshUuids,
        const QString& diffusePath,
        const QString& specularPath,
        const QString& normalPath,
        const QString& emissivePath,
        const QString& heightPath,
        const QString& opacityPath,
        bool opacityInverted);

    void undo() override;
    void redo() override;
    int id() const override { return 12; }
    QSet<QUuid> getReferencedUuids() const;

private:
    struct ADSTextures
    {
        QString diffusePath;
        QString specularPath;
        QString normalPath;
        QString emissivePath;
        QString heightPath;
        QString opacityPath;
        bool opacityInverted;

        // Flags for which textures are enabled
        bool hasDiffuse;
        bool hasSpecular;
        bool hasNormal;
        bool hasEmissive;
        bool hasHeight;
        bool hasOpacity;
    };

    QMap<QUuid, ADSTextures> _oldTextures;
    QMap<QUuid, ADSTextures> _newTextures;

    void applyTextures(const QMap<QUuid, ADSTextures>& textures);
};
