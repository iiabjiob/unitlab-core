from .devices import handle_scan_devices
from .state import handle_get_states
from .do_commands import handle_set_do_command
from .ao_commands import handle_set_ao_command
# from .di_commands import ...

ACTION_HANDLERS = {
    "scan_devices": handle_scan_devices,
    "get_states": handle_get_states,
    "set_do_command": handle_set_do_command,
    "set_ao_command": handle_set_ao_command,
}
