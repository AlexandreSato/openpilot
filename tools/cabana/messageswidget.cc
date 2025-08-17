#include "tools/cabana/messageswidget.h"

#include <limits>
#include <utility>
#include <tuple>

#include <QCheckBox>
#include <QHBoxLayout>
#include <QComboBox>
#include <QLabel>
#include <QPainter>
#include <QPalette>
#include <QPushButton>
#include <QScrollBar>
#include <QVBoxLayout>

#include "tools/cabana/commands.h"

MessagesWidget::MessagesWidget(QWidget *parent) : menu(new QMenu(this)), QWidget(parent) {
  QVBoxLayout *main_layout = new QVBoxLayout(this);
  main_layout->setContentsMargins(0, 0, 0, 0);
  // toolbar
  main_layout->addWidget(createToolBar());
  // message table
  main_layout->addWidget(view = new MessageView(this));
  view->setItemDelegate(delegate = new MessageBytesDelegate(view, settings.multiple_lines_hex));
  view->setModel(model = new MessageListModel(this));
  view->setHeader(header = new MessageViewHeader(this));
  view->setSortingEnabled(true);
  view->sortByColumn(MessageListModel::Column::NAME, Qt::AscendingOrder);
  view->setAllColumnsShowFocus(true);
  view->setEditTriggers(QAbstractItemView::NoEditTriggers);
  view->setItemsExpandable(false);
  view->setIndentation(0);
  view->setRootIsDecorated(false);

  // Must be called before setting any header parameters to avoid overriding
  restoreHeaderState(settings.message_header_state);
  header->setSectionsMovable(true);
  header->setSectionResizeMode(MessageListModel::Column::DATA, QHeaderView::Fixed);
  header->setStretchLastSection(true);
  header->setContextMenuPolicy(Qt::CustomContextMenu);

  // signals/slots
  QObject::connect(menu, &QMenu::aboutToShow, this, &MessagesWidget::menuAboutToShow);
  QObject::connect(header, &MessageViewHeader::customContextMenuRequested, this, &MessagesWidget::headerContextMenuEvent);
  QObject::connect(view->horizontalScrollBar(), &QScrollBar::valueChanged, header, &MessageViewHeader::updateHeaderPositions);
  QObject::connect(can, &AbstractStream::msgsReceived, model, &MessageListModel::msgsReceived);
  QObject::connect(dbc(), &DBCManager::DBCFileChanged, model, &MessageListModel::dbcModified);
  QObject::connect(UndoStack::instance(), &QUndoStack::indexChanged, model, &MessageListModel::dbcModified);
  QObject::connect(model, &MessageListModel::modelReset, [this]() {
    if (current_msg_id) {
      selectMessage(*current_msg_id);
    }
    view->updateBytesSectionSize();
    updateTitle();
  });
  QObject::connect(view->selectionModel(), &QItemSelectionModel::currentChanged, [=](const QModelIndex &current, const QModelIndex &previous) {
    if (current.isValid() && current.row() < model->items_.size()) {
      const auto &id = model->items_[current.row()].id;
      if (!current_msg_id || id != *current_msg_id) {
        current_msg_id = id;
        emit msgSelectionChanged(*current_msg_id);
      }
    }
  });

  setWhatsThis(tr(R"(
    <b>Message View</b><br/>
    <!-- TODO: add descprition here -->
    <span style="color:gray">Byte color</span><br />
    <span style="color:gray;">■ </span> constant changing<br />
    <span style="color:blue;">■ </span> increasing<br />
    <span style="color:red;">■ </span> decreasing<br />
    <span style="color:gray">Shortcuts</span><br />
    Horizontal Scrolling: <span style="background-color:lightGray;color:gray">&nbsp;shift+wheel&nbsp;</span>
  )"));
}

QWidget *MessagesWidget::createToolBar() {
  QWidget *toolbar = new QWidget(this);
  QHBoxLayout *layout = new QHBoxLayout(toolbar);
  layout->setContentsMargins(0, 9, 0, 0);
  layout->addWidget(suppress_add = new QPushButton("Suppress Highlighted"));
  layout->addWidget(suppress_clear = new QPushButton());
  suppress_clear->setToolTip(tr("Clear suppressed"));
  layout->addStretch(1);
  // Demux dropdown
  layout->addWidget(new QLabel(tr("Demux:"), this));
  auto demux_combo = new QComboBox(this);
  demux_combo->addItems({"1", "2", "4", "8", "16", "32"});
  demux_combo->setCurrentText("1");
  layout->addWidget(demux_combo);
  QCheckBox *suppress_defined_signals = new QCheckBox(tr("Suppress Signals"), this);
  suppress_defined_signals->setToolTip(tr("Suppress defined signals"));
  suppress_defined_signals->setChecked(settings.suppress_defined_signals);
  layout->addWidget(suppress_defined_signals);

  auto view_button = new ToolButton("three-dots", tr("View..."));
  view_button->setMenu(menu);
  view_button->setPopupMode(QToolButton::InstantPopup);
  view_button->setStyleSheet("QToolButton::menu-indicator { image: none; }");
  layout->addWidget(view_button);

  QObject::connect(suppress_add, &QPushButton::clicked, this, &MessagesWidget::suppressHighlighted);
  QObject::connect(suppress_clear, &QPushButton::clicked, this, &MessagesWidget::suppressHighlighted);
  QObject::connect(suppress_defined_signals, &QCheckBox::stateChanged, can, &AbstractStream::suppressDefinedSignals);
  QObject::connect(demux_combo, &QComboBox::currentTextChanged, this, [this](const QString &txt){
    bool ok = false;
    int value = txt.toInt(&ok);
    if (ok && value > 0) {
      model->setCycleRepetition(value);
      // force repaint to update displayed IDs even if items_ unchanged
      if (model->rowCount() > 0) {
        emit model->dataChanged(model->index(0, 0), model->index(model->rowCount()-1, model->columnCount()-1));
      }
    }
  });

  suppressHighlighted();
  return toolbar;
}

void MessagesWidget::updateTitle() {
  auto stats = std::accumulate(
      model->items_.begin(), model->items_.end(), std::pair<size_t, size_t>(),
      [](const auto &pair, const auto &item) {
        auto m = dbc()->msg(item.id);
        return m ? std::make_pair(pair.first + 1, pair.second + m->sigs.size()) : pair;
      });
  emit titleChanged(tr("%1 Messages (%2 DBC Messages, %3 Signals)")
                      .arg(model->items_.size()).arg(stats.first).arg(stats.second));
}

void MessagesWidget::selectMessage(const MessageId &msg_id) {
  auto it = std::find_if(model->items_.cbegin(), model->items_.cend(),
                         [&msg_id](auto &item) { return item.id == msg_id; });
  if (it != model->items_.cend()) {
    view->setCurrentIndex(model->index(std::distance(model->items_.cbegin(), it), 0));
  }
}

void MessagesWidget::suppressHighlighted() {
  int n = sender() == suppress_add ? can->suppressHighlighted() : (can->clearSuppressed(), 0);
  suppress_clear->setText(n > 0 ? tr("Clear (%1)").arg(n) : tr("Clear"));
  suppress_clear->setEnabled(n > 0);
}

void MessagesWidget::headerContextMenuEvent(const QPoint &pos) {
  menu->exec(header->mapToGlobal(pos));
}

void MessagesWidget::menuAboutToShow() {
  menu->clear();
  for (int i = 0; i < header->count(); ++i) {
    int logical_index = header->logicalIndex(i);
    auto action = menu->addAction(model->headerData(logical_index, Qt::Horizontal).toString(),
                                  [=](bool checked) { header->setSectionHidden(logical_index, !checked); });
    action->setCheckable(true);
    action->setChecked(!header->isSectionHidden(logical_index));
    // Can't hide the name column
    action->setEnabled(logical_index > 0);
  }
  menu->addSeparator();
  auto action = menu->addAction(tr("Multi-Line bytes"), this, &MessagesWidget::setMultiLineBytes);
  action->setCheckable(true);
  action->setChecked(settings.multiple_lines_hex);

  action = menu->addAction(tr("Show inactive Messages"), model, &MessageListModel::showInactivemessages);
  action->setCheckable(true);
  action->setChecked(model->show_inactive_messages);
}

void MessagesWidget::setMultiLineBytes(bool multi) {
  settings.multiple_lines_hex = multi;
  delegate->setMultipleLines(multi);
  view->updateBytesSectionSize();
  view->doItemsLayout();
}

// MessageListModel

QVariant MessageListModel::headerData(int section, Qt::Orientation orientation, int role) const {
  if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
    switch (section) {
      case Column::NAME: return tr("Name");
      case Column::SOURCE: return tr("Bus");
      case Column::ADDRESS: return tr("ID");
      case Column::NODE: return tr("Node");
      case Column::FREQ: return tr("Freq");
      case Column::COUNT: return tr("Count");
      case Column::DATA: return tr("Bytes");
    }
  }
  return {};
}

// Return demux-specific bytes vector reference for BytesRole rendering
const std::vector<uint8_t> &MessageListModel::demuxBytes(const Item &item) const {
  int repetition = getCycleRepetition();
  if (repetition <= 1 || item.cycle_base < 0) {
    return can->lastMessage(item.id).dat;
  }

  // Demux needs 8-way (or N-way) contiguous sequence alignment.
  // Anchor to the current block base, then pick desired_cycle = base + cycle_base.
  const auto &lastMsg = can->lastMessage(item.id);
  if (lastMsg.dat.empty()) {
    static const std::vector<uint8_t> empty_data;
    return empty_data;
  }
  const int last_cycle = lastMsg.dat[0];
  const int block_base = last_cycle - (last_cycle % repetition);
  const int desired_cycle = block_base + item.cycle_base;

  if (lastMsg.dat[0] == desired_cycle) {
    return lastMsg.dat;
  }

  const auto &evs = can->events(item.id);
  // only consider events up to current playback time to avoid future data
  const uint64_t current_mono = can->toMonoTime(can->currentSec());
  auto it_current = std::upper_bound(evs.begin(), evs.end(), current_mono, CompareCanEvent());

  // search within the current demux block
  for (auto rit = std::make_reverse_iterator(it_current); rit != evs.rend(); ++rit) {
    const CanEvent *e = *rit;
    if (!e || e->size == 0) continue;
    const int cyc = e->dat[0];
    const int cyc_base = cyc - (cyc % repetition);
    if (cyc_base < block_base) break;  // left this block
    if (cyc == desired_cycle) {
      uint64_t key = makeDemuxKey(item);
      auto &dst = demux_bytes_cache_[key];
      dst.assign(e->dat, e->dat + e->size);
      return dst;
    }
  }

  // fallback: search a few previous blocks (never into the future)
  for (int step = 1; step <= 4; ++step) {
    int older_cycle = desired_cycle - step * repetition;
    for (auto rit = std::make_reverse_iterator(it_current); rit != evs.rend(); ++rit) {
      const CanEvent *e = *rit;
      if (e && e->size > 0 && e->dat[0] == older_cycle) {
        uint64_t key = makeDemuxKey(item);
        auto &dst = demux_bytes_cache_[key];
        dst.assign(e->dat, e->dat + e->size);
        return dst;
      }
    }
  }
  static const std::vector<uint8_t> empty_data;
  return empty_data;
}

const std::vector<QColor> &MessageListModel::demuxColors(const Item &item) const {
  // Use the same color scheme as the original CanData::compute
  static const QColor cyan(0, 187, 255, 128);         // start_alpha for increasing values
  static const QColor red(255, 0, 0, 128);            // start_alpha for decreasing values
  static const double fade_alpha_delta = 5.0;         // Simplified fade rate

  uint64_t key = makeDemuxKey(item);
  auto &color_state = demux_color_states_[key];

  // Get the current demuxed bytes
  const auto &current_bytes = demuxBytes(item);
  if (current_bytes.empty()) {
    static const std::vector<QColor> empty_colors;
    return empty_colors;
  }

  // Initialize state if this is the first time we see this demux
  if (color_state.last_data.empty()) {
    color_state.last_data = current_bytes;
    color_state.colors.assign(current_bytes.size(), QColor(0, 0, 0, 0));
    color_state.last_update_time = can->currentSec();
  }

  // Get current time for fade calculations
  double current_time = can->currentSec();
  double time_delta = current_time - color_state.last_update_time;

  // Resize colors vector if needed
  if (color_state.colors.size() != current_bytes.size()) {
    color_state.colors.resize(current_bytes.size(), QColor(0, 0, 0, 0));
  }

  // Compare current data with last known state and update colors
  for (size_t i = 0; i < current_bytes.size(); ++i) {
    if (i < color_state.last_data.size() && current_bytes[i] != color_state.last_data[i]) {
      // Byte changed - set appropriate color
      if (current_bytes[i] > color_state.last_data[i]) {
        color_state.colors[i] = cyan;  // Increasing
      } else {
        color_state.colors[i] = red;   // Decreasing
      }
    } else {
      // Byte unchanged - fade existing color
      QColor &c = color_state.colors[i];
      if (c.alpha() > 0) {
        int new_alpha = std::max(0, c.alpha() - static_cast<int>(fade_alpha_delta * time_delta * 60)); // 60 fps assumed
        c.setAlpha(new_alpha);
      }
    }
  }

  // Update state for next comparison
  color_state.last_data = current_bytes;
  color_state.last_update_time = current_time;

  return color_state.colors;
}

QVariant MessageListModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() >= items_.size()) return {};

  auto getFreq = [](float freq) {
    if (freq > 0) {
      return freq >= 0.95 ? QString::number(std::nearbyint(freq)) : QString::number(freq, 'f', 2);
    } else {
      return QStringLiteral("--");
    }
  };

  const static QString NA = QStringLiteral("N/A");
  const auto &item = items_[index.row()];
  if (role == Qt::DisplayRole) {
    switch (index.column()) {
      case Column::NAME: return item.name;
      case Column::SOURCE: return item.id.source != INVALID_SOURCE ? QString::number(item.id.source) : NA;
      case Column::ADDRESS: {
        if (item.id.source != INVALID_SOURCE) {
          int repetition = getCycleRepetition();
          if (repetition > 1) {
            int cycle_base = std::max(item.cycle_base, 0);
            uint32_t demux_addr = (item.id.address << 8) + static_cast<uint32_t>(cycle_base);
            return toHexString(demux_addr);
          }
        }
        return toHexString(item.id.address);
      }
      case Column::NODE: return item.node;
      case Column::FREQ: {
        if (item.id.source == INVALID_SOURCE) return NA;
        int repetition = getCycleRepetition();
        if (repetition <= 1 || item.cycle_base < 0) return getFreq(can->lastMessage(item.id).freq);
        // Approximate: use overall freq; per-cycle freq requires deeper aggregation
        return getFreq(can->lastMessage(item.id).freq / repetition);
      }
      case Column::COUNT: {
        if (item.id.source == INVALID_SOURCE) return NA;
        int repetition = getCycleRepetition();
        if (repetition <= 1 || item.cycle_base < 0) return QString::number(can->lastMessage(item.id).count);
        // Count per cycle_base by scanning recent events quickly (best-effort)
        const auto &evs = can->events(item.id);
        uint32_t cnt = 0;
        for (const auto *e : evs) {
          if (e && e->size > 0 && (e->dat[0] % repetition) == item.cycle_base) ++cnt;
        }
        return QString::number(cnt);
      }
      case Column::DATA: return item.id.source != INVALID_SOURCE ? "" : NA;
    }
  } else if (role == ColorsRole) {
    if (getCycleRepetition() > 1 && item.cycle_base >= 0) {
      return QVariant::fromValue((void*)(&demuxColors(item)));
    }
    return QVariant::fromValue((void*)(&can->lastMessage(item.id).colors));
  } else if (role == BytesRole && index.column() == Column::DATA && item.id.source != INVALID_SOURCE) {
    return QVariant::fromValue((void*)(&demuxBytes(item)));
  } else if (role == Qt::ToolTipRole && index.column() == Column::NAME) {
    auto msg = dbc()->msg(item.id);
    auto tooltip = item.name;
    if (msg && !msg->comment.isEmpty()) tooltip += "<br /><span style=\"color:gray;\">" + msg->comment + "</span>";
    return tooltip;
  }
  return {};
}

void MessageListModel::setFilterStrings(const QMap<int, QString> &filters) {
  filters_ = filters;
  filterAndSort();
}

void MessageListModel::showInactivemessages(bool show) {
  show_inactive_messages = show;
  filterAndSort();
}

void MessageListModel::dbcModified() {
  dbc_messages_.clear();
  for (const auto &[_, m] : dbc()->getMessages(-1)) {
    dbc_messages_.insert(MessageId{.source = INVALID_SOURCE, .address = m.address});
  }
  filterAndSort();
}

void MessageListModel::sortItems(std::vector<MessageListModel::Item> &items) {
  auto effectiveAddress = [this](const Item &it) -> uint32_t {
    uint32_t addr = it.id.address;
    if (it.id.source != INVALID_SOURCE) {
      int repetition = getCycleRepetition();
      if (repetition > 1) {
        int cycle_base = std::max(it.cycle_base, 0);
        addr = (addr << 8) + static_cast<uint32_t>(cycle_base);
      }
    }
    return addr;
  };

  auto compare = [this, &effectiveAddress](const auto &l, const auto &r) {
    switch (sort_column) {
      case Column::NAME: return std::tie(l.name, l.id) < std::tie(r.name, r.id);
      case Column::SOURCE: return std::make_tuple(l.id.source, effectiveAddress(l)) < std::make_tuple(r.id.source, effectiveAddress(r));
      case Column::ADDRESS: return std::make_tuple(effectiveAddress(l), l.id.source) < std::make_tuple(effectiveAddress(r), r.id.source);
      case Column::NODE: return std::tie(l.node, l.id) < std::tie(r.node, r.id);
      case Column::FREQ: return std::tie(can->lastMessage(l.id).freq, l.id) < std::tie(can->lastMessage(r.id).freq, r.id);
      case Column::COUNT: return std::tie(can->lastMessage(l.id).count, l.id) < std::tie(can->lastMessage(r.id).count, r.id);
      default: return false; // Default case to suppress compiler warning
    }
  };

  if (sort_order == Qt::DescendingOrder)
    std::stable_sort(items.rbegin(), items.rend(), compare);
  else
    std::stable_sort(items.begin(), items.end(), compare);
}

static bool parseRange(const QString &filter, uint32_t value, int base = 10) {
  // Parse out filter string into a range (e.g. "1" -> {1, 1}, "1-3" -> {1, 3}, "1-" -> {1, inf})
  unsigned int min = std::numeric_limits<unsigned int>::min();
  unsigned int max = std::numeric_limits<unsigned int>::max();
  auto s = filter.split('-');
  bool ok = s.size() >= 1 && s.size() <= 2;
  if (ok && !s[0].isEmpty()) min = s[0].toUInt(&ok, base);
  if (ok && s.size() == 1) {
    max = min;
  } else if (ok && s.size() == 2 && !s[1].isEmpty()) {
    max = s[1].toUInt(&ok, base);
  }
  return ok && value >= min && value <= max;
}

bool MessageListModel::match(const MessageListModel::Item &item) {
  if (filters_.isEmpty())
    return true;

  bool match = true;
  const auto &data = can->lastMessage(item.id);
  for (auto it = filters_.cbegin(); it != filters_.cend() && match; ++it) {
    const QString &txt = it.value();
    switch (it.key()) {
      case Column::NAME: {
        match = item.name.contains(txt, Qt::CaseInsensitive);
        if (!match) {
          const auto m = dbc()->msg(item.id);
          match = m && std::any_of(m->sigs.cbegin(), m->sigs.cend(),
                                   [&txt](const auto &s) { return s->name.contains(txt, Qt::CaseInsensitive); });
        }
        break;
      }
      case Column::SOURCE:
        match = parseRange(txt, item.id.source);
        break;
      case Column::ADDRESS: {
        uint32_t addr = item.id.address;
        if (item.id.source != INVALID_SOURCE) {
          int repetition = getCycleRepetition();
          if (repetition > 1) {
            int cycle_base = std::max(item.cycle_base, 0);
            addr = (addr << 8) + static_cast<uint32_t>(cycle_base);
          }
        }
        match = toHexString(addr).contains(txt, Qt::CaseInsensitive);
        match = match || parseRange(txt, addr, 16);
        break;
      }
      case Column::NODE:
        match = item.node.contains(txt, Qt::CaseInsensitive);
        break;
      case Column::FREQ:
        match = parseRange(txt, data.freq);
        break;
      case Column::COUNT:
        match = parseRange(txt, data.count);
        break;
      case Column::DATA:
        match = utils::toHex(data.dat).contains(txt, Qt::CaseInsensitive);
        break;
    }
  }
  return match;
}

bool MessageListModel::filterAndSort() {
  // merge CAN and DBC messages
  std::vector<MessageId> all_messages;
  all_messages.reserve(can->lastMessages().size() + dbc_messages_.size());
  auto dbc_msgs = dbc_messages_;
  for (const auto &[id, m] : can->lastMessages()) {
    all_messages.push_back(id);
    dbc_msgs.erase(MessageId{.source = INVALID_SOURCE, .address = id.address});
  }
  all_messages.insert(all_messages.end(), dbc_msgs.begin(), dbc_msgs.end());

  // filter and sort
  std::vector<Item> items;
  items.reserve(all_messages.size());
  for (const auto &id : all_messages) {
    if (show_inactive_messages || can->isMessageActive(id)) {
      auto msg = dbc()->msg(id);
      int repetition = getCycleRepetition();
      if (repetition > 1 && id.source != INVALID_SOURCE) {
        // generate rows for each cycle_base (0..repetition-1)
        for (int cb = 0; cb < repetition; ++cb) {
          Item item = {.id = id,
                       .name = msg ? msg->name : UNTITLED,
                       .node = msg ? msg->transmitter : QString(),
                       .cycle_base = cb};
          if (match(item)) items.emplace_back(item);
        }
      } else {
        Item item = {.id = id,
                     .name = msg ? msg->name : UNTITLED,
                     .node = msg ? msg->transmitter : QString(),
                     .cycle_base = -1};
        if (match(item)) items.emplace_back(item);
      }
    }
  }
  sortItems(items);

  if (items_ != items) {
    beginResetModel();
    items_ = std::move(items);
    endResetModel();
    return true;
  }
  return false;
}

void MessageListModel::msgsReceived(const std::set<MessageId> *new_msgs, bool has_new_ids) {
  // Clear demux cache when new messages arrive to ensure fresh data
  demux_bytes_cache_.clear();
  demux_colors_cache_.clear();
  // Note: We don't clear demux_color_states_ here as we want to preserve color state between updates

  if (has_new_ids || ((filters_.count(Column::FREQ) || filters_.count(Column::COUNT) || filters_.count(Column::DATA)) &&
                      ++sort_threshold_ == settings.fps)) {
    sort_threshold_ = 0;
    if (filterAndSort()) return;
  }

  // Update viewport
  emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
}

void MessageListModel::sort(int column, Qt::SortOrder order) {
  if (column != Column::DATA) {
    sort_column = column;
    sort_order = order;
    filterAndSort();
  }
}

// MessageView

void MessageView::drawRow(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {
   const auto &item = ((MessageListModel*)model())->items_[index.row()];
  if (!can->isMessageActive(item.id)) {
    QStyleOptionViewItem custom_option = option;
    custom_option.palette.setBrush(QPalette::Text, custom_option.palette.color(QPalette::Disabled, QPalette::Text));
    auto color = QApplication::palette().color(QPalette::HighlightedText);
    color.setAlpha(100);
    custom_option.palette.setBrush(QPalette::HighlightedText, color);
    QTreeView::drawRow(painter, custom_option, index);
  } else {
    QTreeView::drawRow(painter, option, index);
  }

  QPen oldPen = painter->pen();
  const int gridHint = style()->styleHint(QStyle::SH_Table_GridLineColor, &option, this);
  painter->setPen(QColor::fromRgba(static_cast<QRgb>(gridHint)));
  // Draw bottom border for the row
  painter->drawLine(option.rect.bottomLeft(), option.rect.bottomRight());
  // Draw vertical borders for each column
  for (int i = 0; i < header()->count(); ++i) {
    int sectionX = header()->sectionViewportPosition(i);
    painter->drawLine(sectionX, option.rect.top(), sectionX, option.rect.bottom());
  }
  painter->setPen(oldPen);
}

void MessageView::dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles) {
  // Bypass the slow call to QTreeView::dataChanged.
  // QTreeView::dataChanged will invalidate the height cache and that's what we don't need in MessageView.
  QAbstractItemView::dataChanged(topLeft, bottomRight, roles);
}

void MessageView::updateBytesSectionSize() {
  auto delegate = ((MessageBytesDelegate *)itemDelegate());
  int max_bytes = 8;
  if (!delegate->multipleLines()) {
    for (const auto &[_, m] : can->lastMessages()) {
      max_bytes = std::max<int>(max_bytes, m.dat.size());
    }
  }
  setUniformRowHeights(!delegate->multipleLines());
  header()->resizeSection(MessageListModel::Column::DATA, delegate->sizeForBytes(max_bytes).width());
}

void MessageView::wheelEvent(QWheelEvent *event) {
  if (event->modifiers() == Qt::ShiftModifier) {
    QApplication::sendEvent(horizontalScrollBar(), event);
  } else {
    QTreeView::wheelEvent(event);
  }
}

// MessageViewHeader

MessageViewHeader::MessageViewHeader(QWidget *parent) : QHeaderView(Qt::Horizontal, parent) {
  QObject::connect(this, &QHeaderView::sectionResized, this, &MessageViewHeader::updateHeaderPositions);
  QObject::connect(this, &QHeaderView::sectionMoved, this, &MessageViewHeader::updateHeaderPositions);
}

void MessageViewHeader::updateFilters() {
  QMap<int, QString> filters;
  for (int i = 0; i < count(); i++) {
    if (editors[i] && !editors[i]->text().isEmpty()) {
      filters[i] = editors[i]->text();
    }
  }
  qobject_cast<MessageListModel*>(model())->setFilterStrings(filters);
}

void MessageViewHeader::updateHeaderPositions() {
  QSize sz = QHeaderView::sizeHint();
  for (int i = 0; i < count(); i++) {
    if (editors[i]) {
      int h = editors[i]->sizeHint().height();
      editors[i]->setGeometry(sectionViewportPosition(i), sz.height(), sectionSize(i), h);
      editors[i]->setHidden(isSectionHidden(i));
    }
  }
}

void MessageViewHeader::updateGeometries() {
  for (int i = 0; i < count(); i++) {
    if (!editors[i]) {
      QString column_name = model()->headerData(i, Qt::Horizontal, Qt::DisplayRole).toString();
      editors[i] = new QLineEdit(this);
      editors[i]->setClearButtonEnabled(true);
      editors[i]->setPlaceholderText(tr("Filter %1").arg(column_name));

      QObject::connect(editors[i], &QLineEdit::textChanged, this, &MessageViewHeader::updateFilters);
    }
  }
  setViewportMargins(0, 0, 0, editors[0] ? editors[0]->sizeHint().height() : 0);

  QHeaderView::updateGeometries();
  updateHeaderPositions();
}

QSize MessageViewHeader::sizeHint() const {
  QSize sz = QHeaderView::sizeHint();
  return editors[0] ? QSize(sz.width(), sz.height() + editors[0]->height() + 1) : sz;
}
