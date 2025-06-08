import pyray as rl
from openpilot.system.ui.lib.application import gui_app, FontWeight
from openpilot.common.params import Params
from openpilot.system.ui.lib.text_measure import measure_text_cached

# Constantes
FONT_SIZE: int = 60

# class HelloButton(Widget):
class HelloButton:
  def __init__(self, rect: rl.Rectangle):
    super().__init__()
    self._params = Params()
    self._red_color: rl.Color = rl.Color(255, 0, 0, 255)
    self._green_color: rl.Color = rl.Color(0, 255, 0, 255)
    self._rect = rect
    self._font_semi_bold: rl.Font = gui_app.font(FontWeight.SEMI_BOLD)

    self._button_state: bool = False

  def update_state(self) -> None:
    self._button_state = self._params.get_bool("DisengageOnAccelerator")

  def handle_mouse_event(self) -> bool:
    if rl.check_collision_point_rec(rl.get_mouse_position(), self._rect):
      if rl.is_mouse_button_released(rl.MouseButton.MOUSE_BUTTON_LEFT):
        self._params.put_bool("DisengageOnAccelerator", not self._button_state)
      return True
    return False

  def _render(self) -> None:

    text = "NDOG" if self._button_state else "DOG"
    color = self._green_color if self._button_state else self._red_color

    text_width = measure_text_cached(self._font_semi_bold, text, FONT_SIZE).x
    center_x = self._rect.x + ((self._rect.width - text_width) // 2)
    center_y = self._rect.y + ((self._rect.height - FONT_SIZE) // 2)

    rl.draw_text_ex(self._font_semi_bold, text, rl.Vector2(center_x, center_y), FONT_SIZE, 0, color,)
    rl.draw_rectangle_rounded_lines_ex(self._rect, 0.2, 30, 8, color,)

