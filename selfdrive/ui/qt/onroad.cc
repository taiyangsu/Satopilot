#include "selfdrive/ui/qt/onroad.h"

#include <cmath>

#include <QDebug>

#include "common/timing.h"
#include "selfdrive/ui/qt/util.h"

#ifdef ENABLE_MAPS
#include "selfdrive/ui/qt/maps/map.h"
#include "selfdrive/ui/qt/maps/map_helpers.h"
#endif

#define FONT_OPEN_SANS "Inter" //"Open Sans"

OnroadWindow::OnroadWindow(QWidget *parent) : QWidget(parent) {
  QVBoxLayout *main_layout  = new QVBoxLayout(this);
  main_layout->setMargin(bdr_s);
  QStackedLayout *stacked_layout = new QStackedLayout;
  stacked_layout->setStackingMode(QStackedLayout::StackAll);
  main_layout->addLayout(stacked_layout);

  nvg = new AnnotatedCameraWidget(VISION_STREAM_ROAD, this);

  // HelloButton
  buttons = new ButtonsWindow(this);
  // We need activate this function "&ButtonsWindow::updateState" only when button need's receive some update, coming from CarState for example. (This cause "slow frame rate" issue)
  QObject::connect(uiState(), &UIState::uiUpdate, buttons, &ButtonsWindow::updateState);

  stacked_layout->addWidget(buttons);

  QWidget * split_wrapper = new QWidget;
  split = new QHBoxLayout(split_wrapper);
  split->setContentsMargins(0, 0, 0, 0);
  split->setSpacing(0);
  split->addWidget(nvg);

  if (getenv("DUAL_CAMERA_VIEW")) {
    CameraWidget *arCam = new CameraWidget("camerad", VISION_STREAM_ROAD, true, this);
    split->insertWidget(0, arCam);
  }

  if (getenv("MAP_RENDER_VIEW")) {
    CameraWidget *map_render = new CameraWidget("navd", VISION_STREAM_MAP, false, this);
    split->insertWidget(0, map_render);
  }

  stacked_layout->addWidget(split_wrapper);

  alerts = new OnroadAlerts(this);
  alerts->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  stacked_layout->addWidget(alerts);

  // setup stacking order
  alerts->raise();

  setAttribute(Qt::WA_OpaquePaintEvent);
  QObject::connect(uiState(), &UIState::uiUpdate, this, &OnroadWindow::updateState);
  QObject::connect(uiState(), &UIState::offroadTransition, this, &OnroadWindow::offroadTransition);
}

void OnroadWindow::updateState(const UIState &s) {
  QColor bgColor = (Params().getBool("AleSato_SteerAlwaysOn") && s.status != STATUS_ENGAGED)? bg_colors[STATUS_OVERRIDE] : bg_colors[s.status];
  Alert alert = Alert::get(*(s.sm), s.scene.started_frame);
  if (s.sm->updated("controlsState") || !alert.equal({})) {
    if (alert.type == "controlsUnresponsive") {
      bgColor = bg_colors[STATUS_ALERT];
    } else if (alert.type == "controlsUnresponsivePermanent") {
      bgColor = bg_colors[STATUS_DISENGAGED];
    }
    alerts->updateAlert(alert, bgColor);
  }

  if (s.scene.map_on_left) {
    split->setDirection(QBoxLayout::LeftToRight);
  } else {
    split->setDirection(QBoxLayout::RightToLeft);
  }

  nvg->updateState(s);

  if (bg != bgColor) {
    // repaint border
    bg = bgColor;
    update();
  }
}

void OnroadWindow::mousePressEvent(QMouseEvent* e) {
  if (map != nullptr) {
    bool sidebarVisible = geometry().x() > 0;
    map->setVisible(!sidebarVisible && !map->isVisible());
  }
  // propagation event to parent(HomeWindow)
  QWidget::mousePressEvent(e);
}

void OnroadWindow::offroadTransition(bool offroad) {
#ifdef ENABLE_MAPS
  if (!offroad) {
    if (map == nullptr && (uiState()->primeType() || !MAPBOX_TOKEN.isEmpty())) {
      MapWindow * m = new MapWindow(get_mapbox_settings());
      map = m;

      QObject::connect(uiState(), &UIState::offroadTransition, m, &MapWindow::offroadTransition);

      m->setFixedWidth(topWidget(this)->width() / 2);
      split->insertWidget(0, m);

      // Make map visible after adding to split
      m->offroadTransition(offroad);
    }
  }
#endif

  alerts->updateAlert({}, bg);
}

void OnroadWindow::paintEvent(QPaintEvent *event) {
  QPainter p(this);
  p.fillRect(rect(), QColor(bg.red(), bg.green(), bg.blue(), 255));
}

// ***** onroad widgets *****

// HelloButton
ButtonsWindow::ButtonsWindow(QWidget *parent) : QWidget(parent) {
  QVBoxLayout *main_layout  = new QVBoxLayout(this);
  QWidget *btns_wrapper = new QWidget;
  QHBoxLayout *btns_layout  = new QHBoxLayout(btns_wrapper);
  btns_layout->setSpacing(0);
  btns_layout->setContentsMargins(282, 0, 30, 30);
  main_layout->addWidget(btns_wrapper, 0, Qt::AlignBottom);
  QString initHelloButton = "";
  helloButton = new QPushButton(initHelloButton);
  
  QObject::connect(helloButton, &QPushButton::clicked, [=]() {
    bool button_state = Params().getBool("AleSato_SteerAlwaysOn");
    Params().putBool("AleSato_SteerAlwaysOn", !button_state);
    // helloButton->setText(button_state? "Hai!" : "World");
    // helloButton->setStyleSheet(QString("font-size: 45px; border-radius: 100px; border-color: %1").arg(helloButtonColors.at(button_state? 2 : 0)));
  });

  helloButton->setFixedWidth(200);
  helloButton->setFixedHeight(200);
  btns_layout->addWidget(helloButton, 0, Qt::AlignLeft);
  btns_layout->addSpacing(35);  

  setStyleSheet(R"(
    QPushButton {
      color: white;
      text-align: center;
      padding: 0px;
      border-width: 12px;
      border-style: solid;
      background-color: rgba(75, 75, 75, 0.3);
    }
  )");

  helloButton->setStyleSheet(QString("font-size: 45px; border-radius: 100px; border-color: %1").arg(helloButtonColors.at(1)));
}

// We need this function when button need's update from CarState for example
void ButtonsWindow::updateState(const UIState &s) {
  const auto helloButtonState = Params().getBool("AleSato_SteerAlwaysOn");
  if(helloButtonState) {
    helloButton->setStyleSheet(QString("font-size: 45px; border-radius: 100px; border-color: %1").arg(helloButtonColors.at(0)));
    helloButton->setText("STEER\nalways");    
  } else {
    helloButton->setStyleSheet(QString("font-size: 45px; border-radius: 100px; border-color: %1").arg(helloButtonColors.at(2)));
    helloButton->setText("stock"); 
  }
}

// OnroadAlerts
void OnroadAlerts::updateAlert(const Alert &a, const QColor &color) {
  if (!alert.equal(a) || color != bg) {
    alert = a;
    bg = color;
    update();
  }
}

void OnroadAlerts::paintEvent(QPaintEvent *event) {
  if (alert.size == cereal::ControlsState::AlertSize::NONE) {
    return;
  }
  static std::map<cereal::ControlsState::AlertSize, const int> alert_sizes = {
    {cereal::ControlsState::AlertSize::SMALL, 271},
    {cereal::ControlsState::AlertSize::MID, 420},
    {cereal::ControlsState::AlertSize::FULL, height()},
  };
  int h = alert_sizes[alert.size];
  QRect r = QRect(0, height() - h, width(), h);

  QPainter p(this);

  // draw background + gradient
  p.setPen(Qt::NoPen);
  p.setCompositionMode(QPainter::CompositionMode_SourceOver);

  p.setBrush(QBrush(bg));
  p.drawRect(r);

  QLinearGradient g(0, r.y(), 0, r.bottom());
  g.setColorAt(0, QColor::fromRgbF(0, 0, 0, 0.05));
  g.setColorAt(1, QColor::fromRgbF(0, 0, 0, 0.35));

  p.setCompositionMode(QPainter::CompositionMode_DestinationOver);
  p.setBrush(QBrush(g));
  p.fillRect(r, g);
  p.setCompositionMode(QPainter::CompositionMode_SourceOver);

  // text
  const QPoint c = r.center();
  p.setPen(QColor(0xff, 0xff, 0xff));
  p.setRenderHint(QPainter::TextAntialiasing);
  if (alert.size == cereal::ControlsState::AlertSize::SMALL) {
    configFont(p, "Inter", 74, "SemiBold");
    p.drawText(r, Qt::AlignCenter, alert.text1);
  } else if (alert.size == cereal::ControlsState::AlertSize::MID) {
    configFont(p, "Inter", 88, "Bold");
    p.drawText(QRect(0, c.y() - 125, width(), 150), Qt::AlignHCenter | Qt::AlignTop, alert.text1);
    configFont(p, "Inter", 66, "Regular");
    p.drawText(QRect(0, c.y() + 21, width(), 90), Qt::AlignHCenter, alert.text2);
  } else if (alert.size == cereal::ControlsState::AlertSize::FULL) {
    bool l = alert.text1.length() > 15;
    configFont(p, "Inter", l ? 132 : 177, "Bold");
    p.drawText(QRect(0, r.y() + (l ? 240 : 270), width(), 600), Qt::AlignHCenter | Qt::TextWordWrap, alert.text1);
    configFont(p, "Inter", 88, "Regular");
    p.drawText(QRect(0, r.height() - (l ? 361 : 420), width(), 300), Qt::AlignHCenter | Qt::TextWordWrap, alert.text2);
  }
}


ExperimentalButton::ExperimentalButton(QWidget *parent) : QPushButton(parent) {
  setVisible(false);
  setFixedSize(btn_size, btn_size);
  setCheckable(true);

  params = Params();
  engage_img = loadPixmap("../assets/img_chffr_wheel.png", {img_size, img_size});
  experimental_img = loadPixmap("../assets/img_experimental.svg", {img_size, img_size});

  QObject::connect(this, &QPushButton::toggled, [=](bool checked) {
    params.putBool("ExperimentalMode", checked);
  });
}

void ExperimentalButton::updateState(const UIState &s) {
  const SubMaster &sm = *(s.sm);

  // button is "visible" if engageable or enabled
  const auto cs = sm["controlsState"].getControlsState();
  setVisible(cs.getEngageable() || cs.getEnabled());

  // button is "checked" if experimental mode is enabled
  setChecked(sm["controlsState"].getControlsState().getExperimentalMode());

  // disable button when experimental mode is not available, or has not been confirmed for the first time
  const auto cp = sm["carParams"].getCarParams();
  const bool experimental_mode_available = cp.getExperimentalLongitudinalAvailable() ? params.getBool("ExperimentalLongitudinalEnabled") : cp.getOpenpilotLongitudinalControl();
  setEnabled(params.getBool("ExperimentalModeConfirmed") && experimental_mode_available);
}

void ExperimentalButton::paintEvent(QPaintEvent *event) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  QPoint center(btn_size / 2, btn_size / 2);
  QPixmap img = isChecked() ? experimental_img : engage_img;

  p.setOpacity(1.0);
  p.setPen(Qt::NoPen);
  p.setBrush(QColor(0, 0, 0, 166));
  p.drawEllipse(center, btn_size / 2, btn_size / 2);
  p.setOpacity(isDown() ? 0.8 : 1.0);
  p.drawPixmap((btn_size - img_size) / 2, (btn_size - img_size) / 2, img);
 }

AnnotatedCameraWidget::AnnotatedCameraWidget(VisionStreamType type, QWidget* parent) : fps_filter(UI_FREQ, 3, 1. / UI_FREQ), CameraWidget("camerad", type, true, parent) {
  pm = std::make_unique<PubMaster, const std::initializer_list<const char *>>({"uiDebug"});

  QVBoxLayout *main_layout  = new QVBoxLayout(this);
  main_layout->setMargin(bdr_s);
  main_layout->setSpacing(0);

  experimental_btn = new ExperimentalButton(this);
  main_layout->addWidget(experimental_btn, 0, Qt::AlignTop | Qt::AlignRight);

  dm_img = loadPixmap("../assets/img_driver_face.png", {img_size + 5, img_size + 5});
}

void AnnotatedCameraWidget::updateState(const UIState &s) {
  const int SET_SPEED_NA = 255;
  const SubMaster &sm = *(s.sm);

  const bool cs_alive = sm.alive("controlsState");
  const bool nav_alive = sm.alive("navInstruction") && sm["navInstruction"].getValid();

  const auto cs = sm["controlsState"].getControlsState();

  // Handle older routes where vCruiseCluster is not set
  float v_cruise =  cs.getVCruiseCluster() == 0.0 ? cs.getVCruise() : cs.getVCruiseCluster();
  float set_speed = cs_alive ? v_cruise : SET_SPEED_NA;
  bool cruise_set = set_speed > 0 && (int)set_speed != SET_SPEED_NA;
  if (cruise_set && !s.scene.is_metric) {
    set_speed *= KM_TO_MILE;
  }

  // Handle older routes where vEgoCluster is not set
  float v_ego;
  if (sm["carState"].getCarState().getVEgoCluster() == 0.0 && !v_ego_cluster_seen) {
    v_ego = sm["carState"].getCarState().getVEgo();
  } else {
    v_ego = sm["carState"].getCarState().getVEgoCluster();
    v_ego_cluster_seen = true;
  }
  float cur_speed = cs_alive ? std::max<float>(0.0, v_ego) : 0.0;
  cur_speed *= s.scene.is_metric ? MS_TO_KPH : MS_TO_MPH;

  auto speed_limit_sign = sm["navInstruction"].getNavInstruction().getSpeedLimitSign();
  float speed_limit = nav_alive ? sm["navInstruction"].getNavInstruction().getSpeedLimit() : 0.0;
  speed_limit *= (s.scene.is_metric ? MS_TO_KPH : MS_TO_MPH);

  setProperty("speedLimit", speed_limit);
  setProperty("has_us_speed_limit", nav_alive && speed_limit_sign == cereal::NavInstruction::SpeedLimitSign::MUTCD);
  setProperty("has_eu_speed_limit", nav_alive && speed_limit_sign == cereal::NavInstruction::SpeedLimitSign::VIENNA);

  setProperty("is_cruise_set", cruise_set);
  setProperty("is_metric", s.scene.is_metric);
  setProperty("speed", cur_speed);

  setProperty("setSpeed", set_speed);
  setProperty("speedUnit", s.scene.is_metric ? tr("km/h") : tr("mph"));
  setProperty("hideDM", (cs.getAlertSize() != cereal::ControlsState::AlertSize::NONE));
  setProperty("status", s.status);

  // AleSato stuff
  float engine_rpm = sm["carState"].getCarState().getEngineRpm();
  setProperty("enginerpm", engine_rpm);
  setProperty("buttonColorSpeed", engine_rpm > 0);
  float distance_traveled = sm["controlsState"].getControlsState().getDistanceTraveled() / 1000;
  if(!s.scene.is_metric) {distance_traveled *= KM_TO_MILE;}
  setProperty("distanceTraveled", distance_traveled);
  setProperty("left_blinker", s.scene.leftBlinker);
  setProperty("right_blinker", s.scene.rightBlinker);
  setProperty("blinker_rate", s.scene.blinker_blinkingrate);

  // update engageability/experimental mode button
  experimental_btn->updateState(s);

  // update DM icons at 2Hz
  if (sm.frame % (UI_FREQ / 2) == 0) {
    setProperty("dmActive", sm["driverMonitoringState"].getDriverMonitoringState().getIsActiveMode());
    setProperty("rightHandDM", sm["driverMonitoringState"].getDriverMonitoringState().getIsRHD());
  }

  // DM icon transition
  dm_fade_state = fmax(0.0, fmin(1.0, dm_fade_state+0.2*(0.5-(float)(dmActive))));
}

void AnnotatedCameraWidget::drawHud(QPainter &p) {
  p.save();

  // Header gradient
  QLinearGradient bg(0, header_h - (header_h / 2.5), 0, header_h);
  bg.setColorAt(0, QColor::fromRgbF(0, 0, 0, 0.45));
  bg.setColorAt(1, QColor::fromRgbF(0, 0, 0, 0));
  p.fillRect(0, 0, width(), header_h, bg);

  QString speedLimitStr = (speedLimit > 1) ? QString::number(std::nearbyint(speedLimit)) : "–";
  QString speedStr = QString::number(std::nearbyint(speed));
  QString setSpeedStr = is_cruise_set ? QString::number(std::nearbyint(setSpeed)) : "–";

  // Draw outer box + border to contain set speed and speed limit
  int default_rect_width = 172;
  int rect_width = default_rect_width;
  if (is_metric || has_eu_speed_limit) rect_width = 200;
  if (has_us_speed_limit && speedLimitStr.size() >= 3) rect_width = 223;

  int rect_height = 204;
  if (has_us_speed_limit) rect_height = 402;
  else if (has_eu_speed_limit) rect_height = 392;

  int top_radius = 32;
  int bottom_radius = has_eu_speed_limit ? 100 : 32;

  QRect set_speed_rect(60 + default_rect_width / 2 - rect_width / 2, 45, rect_width, rect_height);
  p.setPen(QPen(whiteColor(75), 6));
  p.setBrush(blackColor(166));
  drawRoundedRect(p, set_speed_rect, top_radius, top_radius, bottom_radius, bottom_radius);

  // Draw MAX
  if (is_cruise_set) {
    if (status == STATUS_DISENGAGED) {
      p.setPen(whiteColor());
    } else if (status == STATUS_OVERRIDE) {
      p.setPen(QColor(0x91, 0x9b, 0x95, 0xff));
    } else if (speedLimit > 0) {
      p.setPen(interpColor(
        setSpeed,
        {speedLimit + 5, speedLimit + 15, speedLimit + 25},
        {QColor(0x80, 0xd8, 0xa6, 0xff), QColor(0xff, 0xe4, 0xbf, 0xff), QColor(0xff, 0xbf, 0xbf, 0xff)}
      ));
    } else {
      p.setPen(QColor(0x80, 0xd8, 0xa6, 0xff));
    }
  } else {
    p.setPen(QColor(0xa6, 0xa6, 0xa6, 0xff));
  }
  configFont(p, "Inter", 40, "SemiBold");
  QRect max_rect = getTextRect(p, Qt::AlignCenter, tr("MAX"));
  max_rect.moveCenter({set_speed_rect.center().x(), 0});
  max_rect.moveTop(set_speed_rect.top() + 27);
  p.drawText(max_rect, Qt::AlignCenter, tr("MAX"));

  // Draw set speed
  if (is_cruise_set) {
    if (speedLimit > 0 && status != STATUS_DISENGAGED && status != STATUS_OVERRIDE) {
      p.setPen(interpColor(
        setSpeed,
        {speedLimit + 5, speedLimit + 15, speedLimit + 25},
        {whiteColor(), QColor(0xff, 0x95, 0x00, 0xff), QColor(0xff, 0x00, 0x00, 0xff)}
      ));
    } else {
      p.setPen(whiteColor());
    }
  } else {
    p.setPen(QColor(0x72, 0x72, 0x72, 0xff));
  }
  configFont(p, "Inter", 90, "Bold");
  QRect speed_rect = getTextRect(p, Qt::AlignCenter, setSpeedStr);
  speed_rect.moveCenter({set_speed_rect.center().x(), 0});
  speed_rect.moveTop(set_speed_rect.top() + 77);
  p.drawText(speed_rect, Qt::AlignCenter, setSpeedStr);



  // US/Canada (MUTCD style) sign
  if (has_us_speed_limit) {
    const int border_width = 6;
    const int sign_width = rect_width - 24;
    const int sign_height = 186;

    // White outer square
    QRect sign_rect_outer(set_speed_rect.left() + 12, set_speed_rect.bottom() - 11 - sign_height, sign_width, sign_height);
    p.setPen(Qt::NoPen);
    p.setBrush(whiteColor());
    p.drawRoundedRect(sign_rect_outer, 24, 24);

    // Smaller white square with black border
    QRect sign_rect(sign_rect_outer.left() + 1.5 * border_width, sign_rect_outer.top() + 1.5 * border_width, sign_width - 3 * border_width, sign_height - 3 * border_width);
    p.setPen(QPen(blackColor(), border_width));
    p.setBrush(whiteColor());
    p.drawRoundedRect(sign_rect, 16, 16);

    // "SPEED"
    configFont(p, "Inter", 28, "SemiBold");
    QRect text_speed_rect = getTextRect(p, Qt::AlignCenter, tr("SPEED"));
    text_speed_rect.moveCenter({sign_rect.center().x(), 0});
    text_speed_rect.moveTop(sign_rect_outer.top() + 22);
    p.drawText(text_speed_rect, Qt::AlignCenter, tr("SPEED"));

    // "LIMIT"
    QRect text_limit_rect = getTextRect(p, Qt::AlignCenter, tr("LIMIT"));
    text_limit_rect.moveCenter({sign_rect.center().x(), 0});
    text_limit_rect.moveTop(sign_rect_outer.top() + 51);
    p.drawText(text_limit_rect, Qt::AlignCenter, tr("LIMIT"));

    // Speed limit value
    configFont(p, "Inter", 70, "Bold");
    QRect speed_limit_rect = getTextRect(p, Qt::AlignCenter, speedLimitStr);
    speed_limit_rect.moveCenter({sign_rect.center().x(), 0});
    speed_limit_rect.moveTop(sign_rect_outer.top() + 85);
    p.drawText(speed_limit_rect, Qt::AlignCenter, speedLimitStr);
  }

  // EU (Vienna style) sign
  if (has_eu_speed_limit) {
    int outer_radius = 176 / 2;
    int inner_radius_1 = outer_radius - 6; // White outer border
    int inner_radius_2 = inner_radius_1 - 20; // Red circle

    // Draw white circle with red border
    QPoint center(set_speed_rect.center().x() + 1, set_speed_rect.top() + 204 + outer_radius);
    p.setPen(Qt::NoPen);
    p.setBrush(whiteColor());
    p.drawEllipse(center, outer_radius, outer_radius);
    p.setBrush(QColor(255, 0, 0, 255));
    p.drawEllipse(center, inner_radius_1, inner_radius_1);
    p.setBrush(whiteColor());
    p.drawEllipse(center, inner_radius_2, inner_radius_2);

    // Speed limit value
    int font_size = (speedLimitStr.size() >= 3) ? 60 : 70;
    configFont(p, "Inter", font_size, "Bold");
    QRect speed_limit_rect = getTextRect(p, Qt::AlignCenter, speedLimitStr);
    speed_limit_rect.moveCenter(center);
    p.setPen(blackColor());
    p.drawText(speed_limit_rect, Qt::AlignCenter, speedLimitStr);
  }

  // Begin Ale Sato
  QString engineRPMStr = buttonColorSpeed? QString::number(std::nearbyint(enginerpm)) : "OFF";
  int my_rect_width = 344;
  int my_rect_height = 204;

  int my_top_radius = 32;
  int my_bottom_radius = 32;

  QRect my_set_speed_rect(60, (has_us_speed_limit || has_eu_speed_limit)? 500 : 450, my_rect_width, my_rect_height);
  p.setPen(QPen(whiteColor(75), 6));
  p.setBrush(blackColor(166));
  drawRoundedRect(p, my_set_speed_rect, my_top_radius, my_top_radius, my_bottom_radius, my_bottom_radius);

  // Draw colored ENGINE RPM
  p.setPen(interpColor(
    enginerpm,
    {1500, 2100, 3000},
    {QColor(0x80, 0xd8, 0xa6, 0xff), QColor(0xff, 0xe4, 0xbf, 0xff), QColor(0xff, 0xbf, 0xbf, 0xff)}
  ));
  configFont(p, "Inter", 40, "SemiBold");
  QRect my_max_rect = getTextRect(p, Qt::AlignCenter, tr("ENGINE RPM"));
  my_max_rect.moveCenter({my_set_speed_rect.center().x(), 0});
  my_max_rect.moveTop(my_set_speed_rect.top() + 127);
  p.drawText(my_max_rect, Qt::AlignCenter, tr("ENGINE RPM"));

  // Draw rpm
  if (buttonColorSpeed) {
    if ((speedLimit > 0 && status != STATUS_DISENGAGED && status != STATUS_OVERRIDE) || true) {
      p.setPen(interpColor(
        enginerpm,
        {1500, 2100, 3000},
        {whiteColor(), QColor(0xff, 0x95, 0x00, 0xff), QColor(0xff, 0x00, 0x00, 0xff)}
      ));
    } else {
      p.setPen(whiteColor());
    }
  } else {
    p.setPen(QColor(0x72, 0x72, 0x72, 0xff));
  }
  configFont(p, "Inter", 90, "Bold");
  QRect my_speed_rect = getTextRect(p, Qt::AlignCenter, engineRPMStr);
  my_speed_rect.moveCenter({my_set_speed_rect.center().x(), 0});
  my_speed_rect.moveTop(my_set_speed_rect.top() + 7);
  p.drawText(my_speed_rect, Qt::AlignCenter, engineRPMStr);
  // End AleSato

// Begin2 Ale Sato
  char distanceTraveledStr[16];
  snprintf(distanceTraveledStr, sizeof(distanceTraveledStr), "%.1f", distanceTraveled);

  // Draw outer box + border to contain set speed and speed limit
  int my2_rect_width = 344;
  int my2_rect_height = 204;


  int my2_top_radius = 32;
  int my2_bottom_radius = 32;

  QRect my2_set_speed_rect(rect().right() - 400, 450, my2_rect_width, my2_rect_height);
  p.setPen(QPen(whiteColor(75), 6));
  p.setBrush(blackColor(166));
  drawRoundedRect(p, my2_set_speed_rect, my2_top_radius, my2_top_radius, my2_bottom_radius, my2_bottom_radius);

  // Draw colored TRIP DIST
  p.setPen(interpColor(
    distanceTraveled,
    {3, 5, 10},
    {QColor(0xff, 0xbf, 0xbf, 0xff), QColor(0xff, 0xe4, 0xbf, 0xff), QColor(0x80, 0xd8, 0xa6, 0xff)}
  ));
  configFont(p, "Inter", 40, "SemiBold");
  QRect my2_max_rect = getTextRect(p, Qt::AlignCenter, tr("TRIP DIST"));
  my2_max_rect.moveCenter({my2_set_speed_rect.center().x(), 0});
  my2_max_rect.moveTop(my2_set_speed_rect.top() + 127);
  p.drawText(my2_max_rect, Qt::AlignCenter, tr("TRIP DIST"));

  // Draw trip distance
  if (is_cruise_set || true) {
    if (speedLimit > 0 && status != STATUS_DISENGAGED && status != STATUS_OVERRIDE) {
      p.setPen(interpColor(
        distanceTraveled,
        {3, 5, 10},
        {QColor(0xff, 0x00, 0x00, 0xff), QColor(0xff, 0x95, 0x00, 0xff), whiteColor()}
      ));
    } else {
      p.setPen(whiteColor());
    }
  } else {
    p.setPen(QColor(0x72, 0x72, 0x72, 0xff));
  }
  configFont(p, "Inter", 90, "Bold");
  QRect my2_speed_rect = getTextRect(p, Qt::AlignCenter, distanceTraveledStr);
  my2_speed_rect.moveCenter({my2_set_speed_rect.center().x(), 0});
  my2_speed_rect.moveTop(my2_set_speed_rect.top() + 7);
  p.drawText(my2_speed_rect, Qt::AlignCenter, distanceTraveledStr);
  // End2 AleSato

  // current speed
  configFont(p, "Inter", 230, "Bold");

  // Turning the speed blue
  drawTextWithColor(p, rect().center().x(), 210, speedStr, buttonColorSpeed ? whiteColor() : QColor(20, 255, 20, 255)); 
  

  configFont(p, "Inter", 66, "Regular");
  drawText(p, rect().center().x(), 290, speedUnit, 200);


  // opkr blinker
  if (true) {
    UIState *s = uiState();
    float bw = 0;
    float bx = 0;
    float bh = 0;
    if (left_blinker) {
      bw = 450;
      bx = s->fb_w/2 - bw/2;
      bh = 400;
      QPointF leftbsign1[] = {{bx, bh/4}, {bx-bw/4, bh/4}, {bx-bw/2, bh/2}, {bx-bw/4, bh/4+bh/2}, {bx, bh/4+bh/2}, {bx-bw/4, bh/2}};
      bx -= 125;
      QPointF leftbsign2[] = {{bx, bh/4}, {bx-bw/4, bh/4}, {bx-bw/2, bh/2}, {bx-bw/4, bh/4+bh/2}, {bx, bh/4+bh/2}, {bx-bw/4, bh/2}};
      bx -= 125;
      QPointF leftbsign3[] = {{bx, bh/4}, {bx-bw/4, bh/4}, {bx-bw/2, bh/2}, {bx-bw/4, bh/4+bh/2}, {bx, bh/4+bh/2}, {bx-bw/4, bh/2}};

      if (blinker_rate <= 120 && blinker_rate >= 60) {
        p.setBrush(yellowColor(115));
        p.drawPolygon(leftbsign1, std::size(leftbsign1));
      }
      if (blinker_rate <= 100 && blinker_rate >= 60) {
        p.setBrush(yellowColor(185));
        p.drawPolygon(leftbsign2, std::size(leftbsign2));
      }
      if (blinker_rate <= 80 && blinker_rate >= 60) {
        p.setBrush(yellowColor(255));
        p.drawPolygon(leftbsign3, std::size(leftbsign3));
      }
    }
    if (right_blinker) {
      bw = 450;
      bx = s->fb_w/2 - bw/2 + bw;
      bh = 400;
      QPointF rightbsign1[] = {{bx, bh/4}, {bx+bw/4, bh/4}, {bx+bw/2, bh/2}, {bx+bw/4, bh/4+bh/2}, {bx, bh/4+bh/2}, {bx+bw/4, bh/2}};
      bx += 125;
      QPointF rightbsign2[] = {{bx, bh/4}, {bx+bw/4, bh/4}, {bx+bw/2, bh/2}, {bx+bw/4, bh/4+bh/2}, {bx, bh/4+bh/2}, {bx+bw/4, bh/2}};
      bx += 125;
      QPointF rightbsign3[] = {{bx, bh/4}, {bx+bw/4, bh/4}, {bx+bw/2, bh/2}, {bx+bw/4, bh/4+bh/2}, {bx, bh/4+bh/2}, {bx+bw/4, bh/2}};

      if (blinker_rate <= 120 && blinker_rate >= 60) {
        p.setBrush(yellowColor(115));
        p.drawPolygon(rightbsign1, std::size(rightbsign1));
      }
      if (blinker_rate <= 100 && blinker_rate >= 60) {
        p.setBrush(yellowColor(185));
        p.drawPolygon(rightbsign2, std::size(rightbsign2));
      }
      if (blinker_rate <= 80 && blinker_rate >= 60) {
        p.setBrush(yellowColor(255));
        p.drawPolygon(rightbsign3, std::size(rightbsign3));
      }
    }
  }

  p.restore();
}


// Window that shows camera view and variety of
// info drawn on top

void AnnotatedCameraWidget::drawText(QPainter &p, int x, int y, const QString &text, int alpha) {
  QRect real_rect = getTextRect(p, 0, text);
  real_rect.moveCenter({x, y - real_rect.height() / 2});

  p.setPen(QColor(0xff, 0xff, 0xff, alpha));
  p.drawText(real_rect.x(), real_rect.bottom(), text);
}

void AnnotatedCameraWidget::drawTextWithColor(QPainter &p, int x, int y, const QString &text, QColor color) {
  QRect real_rect = getTextRect(p, 0, text);
  real_rect.moveCenter({x, y - real_rect.height() / 2});

  p.setPen(color);
  p.drawText(real_rect.x(), real_rect.bottom(), text);
}

void AnnotatedCameraWidget::drawIcon(QPainter &p, int x, int y, QPixmap &img, QBrush bg, float opacity) {
  p.setOpacity(1.0);  // bg dictates opacity of ellipse
  p.setPen(Qt::NoPen);
  p.setBrush(bg);
  p.drawEllipse(x - btn_size / 2, y - btn_size / 2, btn_size, btn_size);
  p.setOpacity(opacity);
  p.drawPixmap(x - img.size().width() / 2, y - img.size().height() / 2, img);
  p.setOpacity(1.0);
}


void AnnotatedCameraWidget::initializeGL() {
  CameraWidget::initializeGL();
  qInfo() << "OpenGL version:" << QString((const char*)glGetString(GL_VERSION));
  qInfo() << "OpenGL vendor:" << QString((const char*)glGetString(GL_VENDOR));
  qInfo() << "OpenGL renderer:" << QString((const char*)glGetString(GL_RENDERER));
  qInfo() << "OpenGL language version:" << QString((const char*)glGetString(GL_SHADING_LANGUAGE_VERSION));

  prev_draw_t = millis_since_boot();
  setBackgroundColor(bg_colors[STATUS_DISENGAGED]);
}

void AnnotatedCameraWidget::updateFrameMat() {
  CameraWidget::updateFrameMat();
  UIState *s = uiState();
  int w = width(), h = height();

  s->fb_w = w;
  s->fb_h = h;

  // Apply transformation such that video pixel coordinates match video
  // 1) Put (0, 0) in the middle of the video
  // 2) Apply same scaling as video
  // 3) Put (0, 0) in top left corner of video
  s->car_space_transform.reset();
  s->car_space_transform.translate(w / 2 - x_offset, h / 2 - y_offset)
      .scale(zoom, zoom)
      .translate(-intrinsic_matrix.v[2], -intrinsic_matrix.v[5]);
}

void AnnotatedCameraWidget::drawLaneLines(QPainter &painter, const UIState *s) {
  painter.save();

  const UIScene &scene = s->scene;
  SubMaster &sm = *(s->sm);

  // lanelines
  for (int i = 0; i < std::size(scene.lane_line_vertices); ++i) {
    painter.setBrush(QColor::fromRgbF(1.0, 1.0, 1.0, std::clamp<float>(scene.lane_line_probs[i], 0.0, 0.7)));
    painter.drawPolygon(scene.lane_line_vertices[i]);
  }

  // road edges
  for (int i = 0; i < std::size(scene.road_edge_vertices); ++i) {
    painter.setBrush(QColor::fromRgbF(1.0, 0, 0, std::clamp<float>(1.0 - scene.road_edge_stds[i], 0.0, 1.0)));
    painter.drawPolygon(scene.road_edge_vertices[i]);
  }

  // paint path
  QLinearGradient bg(0, height(), 0, 0);
  if (sm["controlsState"].getControlsState().getExperimentalMode()) {
    // The first half of track_vertices are the points for the right side of the path
    // and the indices match the positions of accel from uiPlan
    const auto &acceleration = sm["uiPlan"].getUiPlan().getAccel();
    const int max_len = std::min<int>(scene.track_vertices.length() / 2, acceleration.size());

    for (int i = 0; i < max_len; ++i) {
      // Some points are out of frame
      if (scene.track_vertices[i].y() < 0 || scene.track_vertices[i].y() > height()) continue;

      // Flip so 0 is bottom of frame
      float lin_grad_point = (height() - scene.track_vertices[i].y()) / height();

      // speed up: 120, slow down: 0
      float path_hue = fmax(fmin(60 + acceleration[i] * 35, 120), 0);
      // FIXME: painter.drawPolygon can be slow if hue is not rounded
      path_hue = int(path_hue * 100 + 0.5) / 100;

      float saturation = fmin(fabs(acceleration[i] * 1.5), 1);
      float lightness = util::map_val(saturation, 0.0f, 1.0f, 0.95f, 0.62f);  // lighter when grey
      float alpha = util::map_val(lin_grad_point, 0.75f / 2.f, 0.75f, 0.4f, 0.0f);  // matches previous alpha fade
      bg.setColorAt(lin_grad_point, QColor::fromHslF(path_hue / 360., saturation, lightness, alpha));

      // Skip a point, unless next is last
      i += (i + 2) < max_len ? 1 : 0;
    }

  } else {
    bg.setColorAt(0.0, QColor::fromHslF(148 / 360., 0.94, 0.51, 0.4));
    bg.setColorAt(0.5, QColor::fromHslF(112 / 360., 1.0, 0.68, 0.35));
    bg.setColorAt(1.0, QColor::fromHslF(112 / 360., 1.0, 0.68, 0.0));
  }

  painter.setBrush(bg);
  painter.drawPolygon(scene.track_vertices);

  painter.restore();
}

void AnnotatedCameraWidget::drawDriverState(QPainter &painter, const UIState *s) {
  const UIScene &scene = s->scene;

  painter.save();

  // base icon
  int x = rightHandDM ? rect().right() -  (btn_size - 24) / 2 - (bdr_s * 2) : (btn_size - 24) / 2 + (bdr_s * 2);
  int y = rect().bottom() - footer_h / 2;
  float opacity = dmActive ? 0.65 : 0.2;
  drawIcon(painter, x, y, dm_img, blackColor(70), opacity);

  // face
  QPointF face_kpts_draw[std::size(default_face_kpts_3d)];
  float kp;
  for (int i = 0; i < std::size(default_face_kpts_3d); ++i) {
    kp = (scene.face_kpts_draw[i].v[2] - 8) / 120 + 1.0;
    face_kpts_draw[i] = QPointF(scene.face_kpts_draw[i].v[0] * kp + x, scene.face_kpts_draw[i].v[1] * kp + y);
  }

  painter.setPen(QPen(QColor::fromRgbF(1.0, 1.0, 1.0, opacity), 5.2, Qt::SolidLine, Qt::RoundCap));
  painter.drawPolyline(face_kpts_draw, std::size(default_face_kpts_3d));

  // tracking arcs
  const int arc_l = 133;
  const float arc_t_default = 6.7;
  const float arc_t_extend = 12.0;
  QColor arc_color = QColor::fromRgbF(0.545 - 0.445 * s->engaged(),
                                      0.545 + 0.4 * s->engaged(),
                                      0.545 - 0.285 * s->engaged(),
                                      0.4 * (1.0 - dm_fade_state));
  float delta_x = -scene.driver_pose_sins[1] * arc_l / 2;
  float delta_y = -scene.driver_pose_sins[0] * arc_l / 2;
  painter.setPen(QPen(arc_color, arc_t_default+arc_t_extend*fmin(1.0, scene.driver_pose_diff[1] * 5.0), Qt::SolidLine, Qt::RoundCap));
  painter.drawArc(QRectF(std::fmin(x + delta_x, x), y - arc_l / 2, fabs(delta_x), arc_l), (scene.driver_pose_sins[1]>0 ? 90 : -90) * 16, 180 * 16);
  painter.setPen(QPen(arc_color, arc_t_default+arc_t_extend*fmin(1.0, scene.driver_pose_diff[0] * 5.0), Qt::SolidLine, Qt::RoundCap));
  painter.drawArc(QRectF(x - arc_l / 2, std::fmin(y + delta_y, y), arc_l, fabs(delta_y)), (scene.driver_pose_sins[0]>0 ? 0 : 180) * 16, 180 * 16);

  painter.restore();
}

void AnnotatedCameraWidget::drawLead(QPainter &painter, const cereal::RadarState::LeadData::Reader &lead_data, const QPointF &vd) {
  painter.save();

  const float speedBuff = 10.;
  const float leadBuff = 40.;
  const float d_rel = lead_data.getDRel();
  const float v_rel = lead_data.getVRel();

  float fillAlpha = 0;
  if (d_rel < leadBuff) {
    fillAlpha = 255 * (1.0 - (d_rel / leadBuff));
    if (v_rel < 0) {
      fillAlpha += 255 * (-1 * (v_rel / speedBuff));
    }
    fillAlpha = (int)(fmin(fillAlpha, 255));
  }

  float sz = std::clamp((25 * 30) / (d_rel / 3 + 30), 15.0f, 30.0f) * 2.35;
  float x = std::clamp((float)vd.x(), 0.f, width() - sz / 2);
  float y = std::fmin(height() - sz * .6, (float)vd.y());

  float g_xo = sz / 5;
  float g_yo = sz / 10;

  // QPointF glow[] = {{x + (sz * 1.35) + g_xo, y + sz + g_yo}, {x, y - g_yo}, {x - (sz * 1.35) - g_xo, y + sz + g_yo}};
  float homebase_h = 12;
  QPointF glow[] = {{x + (sz * 1.35) + g_xo, y + sz + g_yo + homebase_h},{x + (sz * 1.35) + g_xo, y + sz + g_yo}, {x, y - g_yo}, {x - (sz * 1.35) - g_xo, y + sz + g_yo},{x - (sz * 1.35) - g_xo, y + sz + g_yo + homebase_h}, {x, y + sz + homebase_h + g_yo + 10}};
  painter.setBrush(QColor(218, 202, 37, 255));
  painter.drawPolygon(glow, std::size(glow));

  // chevron
  // QPointF chevron[] = {{x + (sz * 1.25), y + sz}, {x, y}, {x - (sz * 1.25), y + sz}};
  QPointF chevron[] = {{x + (sz * 1.25), y + sz + homebase_h},{x + (sz * 1.25), y + sz}, {x, y}, {x - (sz * 1.25), y + sz},{x - (sz * 1.25), y + sz + homebase_h}, {x, y + sz + homebase_h - 7}};
  painter.setBrush(redColor(fillAlpha));
  painter.drawPolygon(chevron, std::size(chevron));

  painter.restore();
}

// Ichirio Stuff
struct LeadcarLockon {
  float x,y,d,a,lxt,lxf,lockOK;
};
#define LeadcarLockon_MAX 5
LeadcarLockon leadcar_lockon[LeadcarLockon_MAX]; //この配列0番を推論1番枠と呼ぶことにする。

void AnnotatedCameraWidget::drawLockon(QPainter &painter, const cereal::ModelDataV2::LeadDataV3::Reader &lead_data, const QPointF &vd , int num  /*使っていない , size_t leads_num , const cereal::RadarState::LeadData::Reader &lead0, const cereal::RadarState::LeadData::Reader &lead1 */) {
  //const float speedBuff = 10.;
  //const float leadBuff = 40.;
  const float d_rel = lead_data.getX()[0];
  //const float d_rel = lead_data.getDRel();
  //const float v_rel = lead_data.getV()[0];
  //const float t_rel = lead_data.getT()[0];
  //const float y_rel = lead_data.getY()[0];
  float a_rel = lead_data.getA()[0];
  //float a_rel = lead_data.getARel(); //ある？
  // global_a_rel = a_rel;

  float sz = std::clamp((25 * 30) / (d_rel / 3 + 30), 15.0f, 30.0f) * 2.35;
  float x = std::clamp((float)vd.x(), 0.f, width() - sz / 2);
  //float y = std::fmin(height() /*- sz * .6*/, (float)vd.y());
  float y = (float)vd.y();

  //float g_xo = sz / 5;
  //float g_yo = sz / 10;

  //QPointF glow[] = {{x + (sz * 1.35) + g_xo, y + sz + g_yo}, {x, y - g_yo}, {x - (sz * 1.35) - g_xo, y + sz + g_yo}};

  painter.setCompositionMode(QPainter::CompositionMode_Plus);
  //p.setPen(QColor(0, 255, 0, 255));

  float prob_alpha = lead_data.getProb(); //getModelProb();
  if(prob_alpha < 0){
    prob_alpha = 0;
  } else if(prob_alpha > 1.0){
    prob_alpha = 1.0;
  }
  prob_alpha *= 245;

  painter.setPen(QPen(QColor(0.09*255, 0.945*255, 0.26*255, prob_alpha), 2));
  painter.setBrush(QColor(0, 0, 0, 0));
  float ww = 300 , hh = 300;
  if(Hardware::TICI()){
    ww *= 1.25; hh *= 1.25;
  }
  float d = d_rel; //距離をロックターケットの大きさに反映させる。
  if(d < 1){
    d = 1;
  }

  //動きに緩衝処理。
  leadcar_lockon[num].x = leadcar_lockon[num].x + (x - leadcar_lockon[num].x) / 6;
  leadcar_lockon[num].y = leadcar_lockon[num].y + (y - leadcar_lockon[num].y) / 6;
  leadcar_lockon[num].d = leadcar_lockon[num].d + (d - leadcar_lockon[num].d) / 6;
  x = leadcar_lockon[num].x;
  y = leadcar_lockon[num].y;
  d = leadcar_lockon[num].d;
  if(d < 1){
    d = 1;
  }

  leadcar_lockon[num].a = leadcar_lockon[num].a + (a_rel - leadcar_lockon[num].a) / 10;
  a_rel = leadcar_lockon[num].a;

  float dh = 50;
  if(uiState()->scene.wide_cam == false) { //dhに奥行き値を反映させる。
    float dd = d;
    dd -= 25; //dd=0〜75
    dd /= (75.0/2); //dd=0〜2
    dd += 1; //dd=1〜3
    if(dd < 1)dd = 1;
    dh /= dd;
  } else { //ワイドカメラ使用でロジック変更。リアルタイムで変わる。
    ww *= 0.5; hh *= 0.5;
    dh = 100;
    float dd = d;
    dd -= 5; //dd=0〜95
    dd /= (95.0/10); //dd=0〜10
    dd += 1; //dd=1〜11
    if(dd < 1)dd = 1;
    dh /= dd*dd;
  }

  ww = ww * 2 * 5 / d;
  hh = hh * 2 * 5 / d;
  y = std::fmin(height() /*- sz * .6*/, y - dh) + dh;
  QRect r = QRect(x - ww/2, y /*- g_yo*/ - hh - dh, ww, hh);

#if 0
  float y0 = lead0.getY()[0];
  float y1 = lead1.getY()[0];
#else
  //y?ってわかりにくいな。横方向なんだが。getYは使えなさそうだし。
  float y0 = leadcar_lockon[0].x * leadcar_lockon[0].d; //こうなったら画面座標から逆算。
  float y1 = leadcar_lockon[1].x * leadcar_lockon[1].d;
#endif

  configFont(painter, FONT_OPEN_SANS, 38, "SemiBold");
  if(num == 0 /* && uiState()->scene.mLockOnButton */){
    //推論1番
    painter.setPen(QPen(QColor(0.09*255, 0.945*255, 0.26*255, prob_alpha), 2));
    painter.drawRect(r);

    //painter.setPen(QPen(QColor(0.09*255, 0.945*255, 0.26*255, prob_alpha), 2));
    if(leadcar_lockon[0].x > leadcar_lockon[1].x - 20){
      leadcar_lockon[num].lxt = leadcar_lockon[num].lxt + (r.right() - leadcar_lockon[num].lxt) / 20;
      leadcar_lockon[num].lxf = leadcar_lockon[num].lxf + (width() - leadcar_lockon[num].lxf) / 20;
      //painter.drawLine(r.right(),r.top() , width() , 0);
    } else {
      leadcar_lockon[num].lxt = leadcar_lockon[num].lxt + (r.left() - leadcar_lockon[num].lxt) / 20;
      leadcar_lockon[num].lxf = leadcar_lockon[num].lxf + (0 - leadcar_lockon[num].lxf) / 20;
      //painter.drawLine(r.left(),r.top() , 0 , 0);
    }
    painter.drawText(r, Qt::AlignTop | Qt::AlignLeft, " " + QString::number(num+1));

    //painter.setPen(QPen(QColor(245, 245, 0, prob_alpha), 2));
    float lxt = leadcar_lockon[num].lxt;
    if(lxt < r.left()){
      lxt = r.left();
    } else if(lxt > r.right()){
      lxt = r.right();
    }
    painter.drawLine(lxt,r.top() , leadcar_lockon[num].lxf , 0);
    if(ww >= 40){
      //painter.drawText(r, Qt::AlignTop | Qt::AlignRight, QString::number((int)(lead_data.getProb()*100)) + "％");

      //num==0のロックオンの右端20ドットくらいをa_rel数値メーターとする。
      painter.setPen(Qt::NoPen);
      float wwa = ww * 0.15;
      if(wwa > 40){
        wwa = 40;
      } else if(wwa < 10){
        wwa = 10;
      }
      if(wwa > ww){
        wwa = ww;
      }

      float hha = 0;
      if(a_rel > 0){
        hha = 1 - 0.1 / a_rel;
        painter.setBrush(QColor(0.09*255, 0.945*255, 0.26*255, prob_alpha*0.9));

        if(hha < 0){
          hha = 0;
        }
        hha = hha * hh;
#if 0
        QRect ra = QRect(x - ww/2 + (ww - wwa), y /*- g_yo*/ - hh - dh + (hh-hha), wwa, hha);
        painter.drawRect(ra);
#else //メーターを斜めに切る
        QPointF meter[] = {{(float)x + ww/2 - wwa/2 - wwa/2 * hha / hh , (float)y /*- g_yo*/ - hh - dh + (hh-hha)},{(float)x + ww/2 , (float)y /*- g_yo*/ - hh - dh + (hh-hha)}, {(float)x + ww/2 , (float)y /*- g_yo*/ - hh - dh + hh}, {(float)x + ww/2 - wwa/2 , (float)y /*- g_yo*/ - hh - dh + hh}};
        painter.drawPolygon(meter, std::size(meter));
#endif
      }
      if(a_rel < 0){
        hha = 1 + 0.1 / a_rel;
        painter.setBrush(QColor(245, 0, 0, prob_alpha));
        //減速は上から下へ変更。
        if(hha < 0){
          hha = 0;
        }
        hha = hha * hh;
#if 0
        QRect ra = QRect(x - ww/2 + (ww - wwa), y /*- g_yo*/ - hh - dh , wwa, hha);
        painter.drawRect(ra);
#else //メーターを斜めに切る
        QPointF meter[] = {{(float)x + ww/2 - wwa/2 , (float)y /*- g_yo*/ - hh - dh},{(float)x + ww/2 , (float)y /*- g_yo*/ - hh - dh}, {(float)x + ww/2 , (float)y /*- g_yo*/ - hh - dh + hha}, {(float)x + ww/2 - wwa/2 - wwa/2 * hha / hh, (float)y /*- g_yo*/ - hh - dh + hha}};
        painter.drawPolygon(meter, std::size(meter));
#endif
      }
    }

    if(//lead0.getX()[0] > lead1.getX()[0] //lead1がlead0より後ろ
        //y0 > y1 //lead1がlead0より左
        std::abs(y0 - y1) <= 300 //大きく横にずれた→逆
        // ||ほかにv_relやa_relで前方の急減速を表示したり（num==0に表示してみた）
        //&& lead1.getX()[0] < 10 //lead1が自分の前10m以内
    ){
      leadcar_lockon[num].lockOK = leadcar_lockon[num].lockOK + (40 - leadcar_lockon[num].lockOK) / 5;
      //float td = 40;
    } else {
      leadcar_lockon[num].lockOK = leadcar_lockon[num].lockOK + (0 - leadcar_lockon[num].lockOK) / 5;
    }
    float td = leadcar_lockon[num].lockOK;
    //d:10〜100->1〜3へ変換
    if(td >= 3){
      float dd = leadcar_lockon[num].d;
      if(dd < 10){
        dd = 10;
      }
      dd -= 10; //dd=0〜90
      dd /= (90.0/2); //dd=0〜2
      dd += 1; //dd=1〜3
      td /= dd;

      float tlw = 8;
      float tlw_2 = tlw / 2;
      painter.setPen(QPen(QColor(0.09*255, 0.945*255, 0.26*255, prob_alpha), tlw));
      painter.drawLine(r.center().x() , r.top()-tlw_2 , r.center().x() , r.top() - td);
      painter.drawLine(r.left()-tlw_2 , r.center().y() , r.left() - td , r.center().y());
      painter.drawLine(r.right()+tlw_2 , r.center().y() , r.right() + td , r.center().y());
      painter.drawLine(r.center().x() , r.bottom()+tlw_2 , r.center().x() , r.bottom() + td);
    }

  } else if(/* uiState()->scene.mLockOnButton */ true){
    if(num == 1){
      //推論2番
      //邪魔な前右寄りを走るバイクを認識したい。
      if(//lead0.getX()[0] > lead1.getX()[0] //lead1がlead0より後ろ
        //y0 > y1 //lead1がlead0より左
        std::abs(y0 - y1) > 300 //大きく横にずれた
        // ||ほかにv_relやa_relで前方の急減速を表示したり（num==0に表示してみた）
        //&& lead1.getX()[0] < 10 //lead1が自分の前10m以内
      ){
        //painter.setPen(QPen(QColor(245, 0, 0, prob_alpha), 4));
        //painter.drawEllipse(r); //縁を描く
        //painter.setPen(QPen(QColor(0.09*255, 0.945*255, 0.26*255, prob_alpha), 1)); //文字を後で書くために色を再設定。->文字は赤でもいいや

        //円を（意味不明だから）書かないで、枠ごと赤くする。推論1が推論と別のものを捉えてるのを簡単に認識できる。
        painter.setPen(QPen(QColor(245, 0, 0, prob_alpha), 2));
      } else {
        painter.setPen(QPen(QColor(0.09*255, 0.945*255, 0.26*255, prob_alpha), 2));
      }

      if(leadcar_lockon[0].x > leadcar_lockon[1].x - 20){ //多少逆転しても許容する
        leadcar_lockon[num].lxt = leadcar_lockon[num].lxt + (r.left() - leadcar_lockon[num].lxt) / 20;
        leadcar_lockon[num].lxf = leadcar_lockon[num].lxf + (0 - leadcar_lockon[num].lxf) / 20;
        //painter.drawLine(r.left(),r.top() , 0 , 0);
      } else {
        leadcar_lockon[num].lxt = leadcar_lockon[num].lxt + (r.right() - leadcar_lockon[num].lxt) / 20;
        leadcar_lockon[num].lxf = leadcar_lockon[num].lxf + (width() - leadcar_lockon[num].lxf) / 20;
        //painter.drawLine(r.right(),r.top() , width() , 0);
      }
      float lxt = leadcar_lockon[num].lxt;
      if(lxt < r.left()){
        lxt = r.left();
      } else if(lxt > r.right()){
        lxt = r.right();
      }
      painter.drawLine(lxt,r.top() , leadcar_lockon[num].lxf , 0);

      if(ww >= 80){
        //float dy = y0 - y1;
        //painter.drawText(r, Qt::AlignBottom | Qt::AlignLeft, " " + QString::number(dy,'f',1) + "m");
        //painter.drawText(r, Qt::AlignBottom | Qt::AlignLeft, " " + QString::number(dy,'f',1));
      }
    } else if(num == 2){
      //推論3番
      //事実上ない。動かない0,0に居るみたい？
      painter.setPen(QPen(QColor(0.09*255, 0.945*255, 0.26*255, prob_alpha), 2));
      //painter.drawLine(r.right(),r.center().y() , width() , height());
    } else {
      //推論4番以降。
      //存在していない。
      painter.setPen(QPen(QColor(0.09*255, 0.945*255, 0.26*255, prob_alpha), 2));
      //painter.drawLine(r.left(),r.center().y() , 0 , height());
    }

    painter.drawRect(r);

    //painter.setPen(QPen(QColor(0.09*255, 0.945*255, 0.26*255, prob_alpha), 2));

    if(ww >= 80){
      //ここではy0,y1を参照できない。
      float d_lim = 12;
      if(wide_cam_requested == false){
        d_lim = 32; //ロングカメラだとちょっと枠が大きい。実測
      }
      if(num == 0 || (num==1 && (d_rel < d_lim || std::abs(y0 - y1) > 300))){ //num==1のとき、'2'の表示と前走車速度表示がかぶるので、こちらを消す。
        painter.drawText(r, Qt::AlignBottom | Qt::AlignLeft, " " + QString::number(num+1));
      }
    }
    if(ww >= 160 /*80*/){
      //painter.drawText(r, Qt::AlignBottom | Qt::AlignRight, QString::number((int)(lead_data.getProb()*100)) + "％");
      //painter.drawText(r, Qt::AlignBottom | Qt::AlignRight, QString::number(a_rel,'f',1) + "a");
    }
  }
  painter.setPen(Qt::NoPen);
  painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
}

void AnnotatedCameraWidget::paintGL() {
  UIState *s = uiState();
  SubMaster &sm = *(s->sm);
  const double start_draw_t = millis_since_boot();
  const cereal::ModelDataV2::Reader &model = sm["modelV2"].getModelV2();
  const cereal::RadarState::Reader &radar_state = sm["radarState"].getRadarState();

  // draw camera frame
  {
    std::lock_guard lk(frame_lock);

    if (frames.empty()) {
      if (skip_frame_count > 0) {
        skip_frame_count--;
        qDebug() << "skipping frame, not ready";
        return;
      }
    } else {
      // skip drawing up to this many frames if we're
      // missing camera frames. this smooths out the
      // transitions from the narrow and wide cameras
      skip_frame_count = 5;
    }

    // Wide or narrow cam dependent on speed
    bool has_wide_cam = available_streams.count(VISION_STREAM_WIDE_ROAD);
    if (has_wide_cam) {
      float v_ego = sm["carState"].getCarState().getVEgo();
      if ((v_ego < 10) || available_streams.size() == 1) {
        wide_cam_requested = true;
      } else if (v_ego > 15) {
        wide_cam_requested = false;
      }
      wide_cam_requested = wide_cam_requested && sm["controlsState"].getControlsState().getExperimentalMode();
      // for replay of old routes, never go to widecam
      wide_cam_requested = wide_cam_requested && s->scene.calibration_wide_valid;
    }
    CameraWidget::setStreamType(wide_cam_requested ? VISION_STREAM_WIDE_ROAD : VISION_STREAM_ROAD);

    // Ale Sato stream_driver when reverse gear
    bool reversing = int(sm["carState"].getCarState().getGearShifter()) == 4;
    if(reversing) {
      CameraWidget::setStreamType(VISION_STREAM_DRIVER);
    }

    s->scene.wide_cam = CameraWidget::getStreamType() == VISION_STREAM_WIDE_ROAD;
    if (s->scene.calibration_valid) {
      auto calib = s->scene.wide_cam ? s->scene.view_from_wide_calib : s->scene.view_from_calib;
      CameraWidget::updateCalibration(calib);
    } else {
      CameraWidget::updateCalibration(DEFAULT_CALIBRATION);
    }
    CameraWidget::setFrameId(model.getFrameId());
    CameraWidget::paintGL();
  }

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setPen(Qt::NoPen);

  if (s->worldObjectsVisible()) {
    if (sm.rcv_frame("modelV2") > s->scene.started_frame) {
      update_model(s, sm["modelV2"].getModelV2(), sm["uiPlan"].getUiPlan());
      if (sm.rcv_frame("radarState") > s->scene.started_frame) {
        update_leads(s, radar_state, sm["modelV2"].getModelV2().getPosition());
      }
    }

    drawLaneLines(painter, s);

    if (s->scene.longitudinal_control) {
      // Ichiro Stuff
      const auto leads = model.getLeadsV3();
      size_t leads_num = leads.size();
      for(size_t i=0; i<leads_num && i < LeadcarLockon_MAX; i++){
        if(leads[i].getProb() > .2){ //信用度20%以上で表示。調整中。
          drawLockon(painter, leads[i], s->scene.lead_vertices[i] , i /*, leads_num , leads[0] , leads[1]*/);
        }
      }
      auto lead_one = radar_state.getLeadOne();
      auto lead_two = radar_state.getLeadTwo();
      if (lead_one.getStatus()) {
        drawLead(painter, lead_one, s->scene.lead_vertices[0]);
      }
      if (lead_two.getStatus() && (std::abs(lead_one.getDRel() - lead_two.getDRel()) > 3.0)) {
        drawLead(painter, lead_two, s->scene.lead_vertices[1]);
      }
    }
  }

  // DMoji
  if (!hideDM && (sm.rcv_frame("driverStateV2") > s->scene.started_frame)) {
    update_dmonitoring(s, sm["driverStateV2"].getDriverStateV2(), dm_fade_state, rightHandDM);
    drawDriverState(painter, s);
  }

  drawHud(painter);

  double cur_draw_t = millis_since_boot();
  double dt = cur_draw_t - prev_draw_t;
  double fps = fps_filter.update(1. / dt * 1000);
  if (fps < 15) {
    LOGW("slow frame rate: %.2f fps", fps);
  }
  prev_draw_t = cur_draw_t;

  // publish debug msg
  MessageBuilder msg;
  auto m = msg.initEvent().initUiDebug();
  m.setDrawTimeMillis(cur_draw_t - start_draw_t);
  pm->send("uiDebug", msg);
}

void AnnotatedCameraWidget::showEvent(QShowEvent *event) {
  CameraWidget::showEvent(event);

  ui_update_params(uiState());
  prev_draw_t = millis_since_boot();
}
