from dragonpilot.settings import tr

ITEMS = [
  {
    "section": "Toyota / Lexus",
    "key": "as_shutdown_with_fog",
    "type": "toggle_item",
    "title": lambda: tr("Apagar a tela com farol de neblina"),
    "description": lambda: tr("Apague a tela do comma quando o farol de neblina estiver ligado."),
    "brands": ["toyota"],
    "flags": "PERSISTENT",
    "param_type": "BOOL",
    "default": "0",
  },
]
