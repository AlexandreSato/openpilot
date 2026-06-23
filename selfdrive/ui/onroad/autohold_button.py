import pyray as rl
from openpilot.system.ui.lib.application import gui_app, FontWeight
from openpilot.common.params import Params
from openpilot.system.ui.widgets import Widget
from openpilot.system.ui.lib.text_measure import measure_text_cached

# Constantes
FONT_SIZE: int = 60

class AutoHoldButton(Widget):
  def __init__(self, button_width: int, button_height: int):
    super().__init__()
    self._params = Params()
    self._disabled_color: rl.Color = rl.Color(129, 34, 166, 255)
    self._green_color: rl.Color = rl.Color(78, 201, 176, 255)
    self._rect = rl.Rectangle(0, 0, button_width, button_height)
    self._font_semi_bold: rl.Font = gui_app.font(FontWeight.SEMI_BOLD)

    self._button_state: bool = False

  def set_rect(self, rect: rl.Rectangle) -> None:
    self._rect.x, self._rect.y = rect.x, rect.y

  def _update_state(self) -> None:
    self._button_state = self._params.get_bool("as_autohold")

  def _handle_mouse_release(self, _):
    super()._handle_mouse_release(_)
    self._params.put_bool("as_autohold", not self._button_state)

  def _render(self, rect: rl.Rectangle) -> None:
    text = "Auto\nHold" if self._button_state else "Auto\nHold"
    color = self._green_color if self._button_state else self._disabled_color

    text_width = measure_text_cached(self._font_semi_bold, text, FONT_SIZE).x
    center_x = self._rect.x + ((self._rect.width - text_width) // 2)
    # center_y = self._rect.y + ((self._rect.height - FONT_SIZE) // 2)
    center_y = self._rect.y + ((self._rect.height - FONT_SIZE) // 8)

    rl.draw_text_ex(self._font_semi_bold, text, rl.Vector2(center_x, center_y), FONT_SIZE, 0, color)
    rl.draw_rectangle_rounded_lines_ex(self._rect, 0.5, 30, 8, color)