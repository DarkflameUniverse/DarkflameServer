"""The chat services the bridge can talk to. Add a new one here (and a module next to these)."""
from .base import Adapter
from .console import ConsoleAdapter
from .discord import DiscordAdapter
from .webhook import WebhookAdapter

ADAPTERS: dict[str, type[Adapter]] = {
    ConsoleAdapter.name: ConsoleAdapter,
    DiscordAdapter.name: DiscordAdapter,
    WebhookAdapter.name: WebhookAdapter,
    # "matrix": MatrixAdapter, "irc": IrcAdapter, ...
}
