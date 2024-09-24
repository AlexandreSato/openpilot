#pragma once

#include <QVBoxLayout>
#include <memory>
#include "selfdrive/ui/qt/onroad/hud.h"
#include "selfdrive/ui/qt/onroad/buttons.h"
#include "selfdrive/ui/qt/onroad/driver_monitoring.h"
#include "selfdrive/ui/qt/widgets/cameraview.h"

// AleSato
class ButtonsWindow : public QWidget {
  Q_OBJECT

  public:
    ButtonsWindow(QWidget* parent = 0);

  private:
    QPushButton *helloButton;
    const QStringList helloButtonColors = {"#37b868", "#fcff4b", "#24a8bc", "#173349"};

  public slots:
    void updateState(const UIState &s);
};
// End AleSato

class AnnotatedCameraWidget : public CameraWidget {
  Q_OBJECT

public:
  explicit AnnotatedCameraWidget(VisionStreamType type, QWidget* parent = 0);
  void updateState(const UIState &s);

private:
  QVBoxLayout *main_layout;
  ExperimentalButton *experimental_btn;
  DriverMonitorRenderer dmon;
  HudRenderer hud;
  std::unique_ptr<PubMaster> pm;

  int skip_frame_count = 0;
  bool wide_cam_requested = false;


  // AleSato stuff
  ButtonsWindow *buttons;


protected:
  void paintGL() override;
  void initializeGL() override;
  void showEvent(QShowEvent *event) override;
  mat4 calcFrameMatrix() override;
  void drawLaneLines(QPainter &painter, const UIState *s);
  void drawLead(QPainter &painter, const cereal::RadarState::LeadData::Reader &lead_data, const QPointF &vd);
  inline QColor redColor(int alpha = 255) { return QColor(201, 34, 49, alpha); }


  // Ichiro Stuff
  void drawLockon(QPainter &painter, const cereal::ModelDataV2::LeadDataV3::Reader &lead_data, const QPointF &vd , int num  /*使っていない, size_t leads_num , const cereal::RadarState::LeadData::Reader &lead0, const cereal::RadarState::LeadData::Reader &lead1 */);
  // End Ichiro Stuff

  double prev_draw_t = 0;
  FirstOrderFilter fps_filter;
};
