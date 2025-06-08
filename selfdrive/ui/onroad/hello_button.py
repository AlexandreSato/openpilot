import pyray as rl
from openpilot.system.ui.lib.application import Widget


class HelloButton(Widget):
  def __init__(self, button_size: int):
    super().__init__()

    self._white_color: rl.Color = rl.Color(255, 255, 255, 255)
    self._black_bg: rl.Color = rl.Color(0, 0, 0, 166)
    self._rect: rl.Rectangle = rl.Rectangle(0, 0, button_size, button_size)

  def update_state(self) -> None:
    pass

  def handle_mouse_event(self) -> bool:
    if rl.check_collision_point_rec(rl.get_mouse_position(), self._rect):
      if rl.is_mouse_button_released(rl.MouseButton.MOUSE_BUTTON_LEFT):
        pass
      return True
    return False

  def _render(self, rect: rl.Rectangle) -> None:
    self._rect.x, self._rect.y = rect.x, rect.y
    center_x = int(self._rect.x + self._rect.width // 2)
    center_y = int(self._rect.y + self._rect.height // 2)

    mouse_over = rl.check_collision_point_rec(rl.get_mouse_position(), self._rect)
    mouse_down = rl.is_mouse_button_down(rl.MouseButton.MOUSE_BUTTON_LEFT) and self._is_pressed
    self._white_color.a = 180 if (mouse_down and mouse_over) else 255

    # rl.draw_circle(center_x, center_y, self._rect.width / 2, self._black_bg)
    rl.draw_rectangle_rounded_lines_ex(rect, 0.2, 30, 6, rl.Color(255, 0, 0, 255))

