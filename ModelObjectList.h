#ifndef MODELOBJECTLIST_H
#define MODELOBJECTLIST_H

#include <QListWidget>

class ModelObjectList : public QListWidget
{
	Q_OBJECT

public:
	ModelObjectList(QWidget* parent = nullptr);

	void filterItems(const QString& filter);

protected slots:
	void keyPressEvent(QKeyEvent* event);

signals:
	void selectionUpdated();

private:
    int levenshteinDistance(const QString &s1, const QString &s2) const;
};

#endif // MODELOBJECTLIST_H
