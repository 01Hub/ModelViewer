#pragma once

#include <QUndoCommand>

// Forward declarations
class ModelViewer;
class GLWidget;

/**
 * @brief Base class for all undoable commands in ModelViewer
 *
 * This provides a common interface for commands that operate on ModelViewer
 * and its associated GLWidget. Derived classes implement specific operations
 * like selection changes, material application, transformations, etc.
 */
class ModelViewerCommand : public QUndoCommand
{
public:
    /**
     * @brief Construct a ModelViewer command
     * @param viewer The ModelViewer instance
     * @param glWidget The GLWidget instance
     * @param text Description of the command for undo/redo menu
     */
    explicit ModelViewerCommand(ModelViewer* viewer,
        GLWidget* glWidget,
        const QString& text = QString());

    virtual ~ModelViewerCommand() = default;

    virtual bool affectsDocument() const { return true; }

protected:
    ModelViewer* _viewer;
    GLWidget* _glWidget;
};

