#include "ModelObjectList.h"
#include <QKeyEvent>

ModelObjectList::ModelObjectList(QWidget* parent) : QListWidget(parent)
{
    setSelectionMode(QAbstractItemView::ExtendedSelection);
}

void ModelObjectList::filterItems(const QString& filter) {
    blockSignals(true);
    clearSelection();

    if (filter.isEmpty()) {
        blockSignals(false);
        return;
    }

    QString lowerFilter = filter.toLower();
    QList<QPair<QListWidgetItem*, int>> scoredItems;

    for (int i = 0; i < count(); ++i) {
        QListWidgetItem* item = this->item(i);
        QString name = item->text().toLower();

        if (name.contains(lowerFilter)) {
            item->setSelected(true);  // Direct match
        }
        else {
            int score = levenshteinDistance(lowerFilter, name);
            scoredItems.append({ item, score });
        }
    }

    // Optionally select closest match (smallest score)
    if (!scoredItems.isEmpty()) {
        auto closest = std::min_element(scoredItems.begin(), scoredItems.end(),
            [](const auto& a, const auto& b) { return a.second < b.second; });
        if (closest != scoredItems.end() && closest->second <= 3) { // Only highlight if decently close
            closest->first->setSelected(true);
        }
    }

    blockSignals(false);

    emit selectionUpdated();
}

// Simple Levenshtein distance (for fuzzy matching)
int ModelObjectList::levenshteinDistance(const QString& s1, const QString& s2) const {
    const int len1 = s1.size(), len2 = s2.size();
    QVector<QVector<int>> d(len1 + 1, QVector<int>(len2 + 1));

    for (int i = 0; i <= len1; ++i) d[i][0] = i;
    for (int j = 0; j <= len2; ++j) d[0][j] = j;

    for (int i = 1; i <= len1; ++i) {
        for (int j = 1; j <= len2; ++j) {
            int cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
            d[i][j] = std::min({
                d[i - 1][j] + 1,
                d[i][j - 1] + 1,
                d[i - 1][j - 1] + cost
                });
        }
    }
    return d[len1][len2];
}

void ModelObjectList::keyPressEvent(QKeyEvent* event)
{
	if (event->key() == Qt::Key_Space)
	{
		QList<QListWidgetItem*> selectedItems = this->selectedItems();
		if (selectedItems.size())
		{
			for (QListWidgetItem* item : selectedItems)
			{
				item->setCheckState(item->checkState() == Qt::Checked ? Qt::Unchecked : Qt::Checked);
			}
		}
	}
	else
	{
		QListWidget::keyPressEvent(event);
	}
}