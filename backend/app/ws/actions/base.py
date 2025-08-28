from .subscriptions import handle_subscribe, handle_unsubscribe
from .devices import handle_scan_devices
from .state import handle_get_states
from .do_commands import handle_set_do_command
from .ao_commands import handle_set_ao_command
# from .di_commands import ...

ACTION_HANDLERS = {
    "subscribe": handle_subscribe,
    "unsubscribe": handle_unsubscribe,
    "scan_devices": handle_scan_devices,
    "get_states": handle_get_states,
    "set_do_command": handle_set_do_command,
    "set_ao_command": handle_set_ao_command,
}
