#pragma once

#include <algorithm>
#include <optional>
#include <set>
#include <vector>
#include <unordered_map>

#include <QAbstractTableModel>
#include <QHeaderView>
#include <QLineEdit>
#include <QMenu>
#include <QTreeView>
#include <QWheelEvent>

#include "tools/cabana/dbc/dbcmanager.h"
#include "tools/cabana/streams/abstractstream.h"

class MessageListModel : public QAbstractTableModel {
Q_OBJECT

public:
  enum Column {
    NAME = 0,
    SOURCE,
    ADDRESS,
    NODE,
    FREQ,
    COUNT,
    DATA,
  };

  MessageListModel(QObject *parent) : QAbstractTableModel(parent) {}
  QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
  int columnCount(const QModelIndex &parent = QModelIndex()) const override { return Column::DATA + 1; }
  QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
  int rowCount(const QModelIndex &parent = QModelIndex()) const override { return items_.size(); }
  void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;
  void setFilterStrings(const QMap<int, QString> &filters);
  void showInactivemessages(bool show);
  void msgsReceived(const std::set<MessageId> *new_msgs, bool has_new_ids);
  bool filterAndSort();
  void dbcModified();
  void setCycleRepetition(int repetition) {
    cycle_repetition = std::max(1, repetition);
    filterAndSort();
  }
  int getCycleRepetition() const { return cycle_repetition; }

  struct Item {
    MessageId id;
    QString name;
    QString node;
    int cycle_base = -1; // -1 when not demuxing
    bool operator==(const Item &other) const {
      return id == other.id && name == other.name && node == other.node && cycle_base == other.cycle_base;
    }
  };
  std::vector<Item> items_;
  bool show_inactive_messages = true;

private:
  struct DemuxColorState {
    std::vector<uint8_t> last_data;
    std::vector<QColor> colors;
    double last_update_time = 0.0;
  };

  void sortItems(std::vector<MessageListModel::Item> &items);
  bool match(const MessageListModel::Item &id);
  uint64_t makeDemuxKey(const Item &item) const {
    return (static_cast<uint64_t>(item.id.source) << 56) | (static_cast<uint64_t>(item.id.address) << 8) | static_cast<uint64_t>(std::max(item.cycle_base, 0));
  }
  const std::vector<uint8_t> &demuxBytes(const Item &item) const;
  const std::vector<QColor> &demuxColors(const Item &item) const;

  QMap<int, QString> filters_;
  std::set<MessageId> dbc_messages_;
  int sort_column = 0;
  Qt::SortOrder sort_order = Qt::AscendingOrder;
  int sort_threshold_ = 0;
  int cycle_repetition = 1;
  mutable std::unordered_map<uint64_t, std::vector<uint8_t>> demux_bytes_cache_;
  mutable std::unordered_map<uint64_t, std::vector<QColor>> demux_colors_cache_;
  mutable std::unordered_map<uint64_t, DemuxColorState> demux_color_states_;
};

class MessageView : public QTreeView {
  Q_OBJECT
public:
  MessageView(QWidget *parent) : QTreeView(parent) {}
  void updateBytesSectionSize();

protected:
  void drawRow(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
  void drawBranches(QPainter *painter, const QRect &rect, const QModelIndex &index) const override {}
  void dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles = QVector<int>()) override;
  void wheelEvent(QWheelEvent *event) override;
};

class MessageViewHeader : public QHeaderView {
  // https://stackoverflow.com/a/44346317
  Q_OBJECT
public:
  MessageViewHeader(QWidget *parent);
  void updateHeaderPositions();
  void updateGeometries() override;
  QSize sizeHint() const override;
  void updateFilters();

  QMap<int, QLineEdit *> editors;
};

class MessagesWidget : public QWidget {
  Q_OBJECT

public:
  MessagesWidget(QWidget *parent);
  void selectMessage(const MessageId &message_id);
  QByteArray saveHeaderState() const { return view->header()->saveState(); }
  bool restoreHeaderState(const QByteArray &state) const { return view->header()->restoreState(state); }
  void suppressHighlighted();

signals:
  void msgSelectionChanged(const MessageId &message_id);
  void titleChanged(const QString &title);

protected:
  QWidget *createToolBar();
  void headerContextMenuEvent(const QPoint &pos);
  void menuAboutToShow();
  void setMultiLineBytes(bool multi);
  void updateTitle();

  MessageView *view;
  MessageViewHeader *header;
  MessageBytesDelegate *delegate;
  std::optional<MessageId> current_msg_id;
  MessageListModel *model;
  QPushButton *suppress_add;
  QPushButton *suppress_clear;
  QMenu *menu;
};
