import sys
import asyncio
from textual.app import App, ComposeResult
from textual.widgets import Input, Header, Footer, RichLog
from textual.containers import Container

sys.path.append("build-debug/py")
import miniexchange_client  # type: ignore


class MiniExchangeClient(App):
    """A TUI for the MiniExchange trading client."""
    
    CSS = """
    #echo_panel {
        height: 1fr;
        border: solid green;
        padding: 1;
        overflow-y: auto;
    }
    
    #input_container {
        height: auto;
        padding: 1;
    }
    
    Input {
        margin: 0;
    }
    """
    
    BINDINGS = [("ctrl+c", "quit", "Quit")]
    
    def __init__(self):
        super().__init__()
        self.client = None
        self.hello_ack_received = False
        
    def compose(self) -> ComposeResult:
        """Create child widgets for the app."""
        yield Header()
        yield RichLog(id="echo_panel", wrap=True, highlight=True, markup=True)
        yield Container(
            Input(placeholder="Enter Commands ... ('help' to see all commands)"),
            id="input_container"
        )
        yield Footer()
    
    def on_mount(self) -> None:
        """Initialize the client when app starts."""
        log = self.query_one("#echo_panel", RichLog)
        log.write("[bold green]Starting Trading Client...[/bold green]")
        
        hmac_key = [0x11] * 32
        api_key = [0x22] * 16
        
        self.client = miniexchange_client.MiniExchangeClient(hmac_key, api_key)
        
        self.client.on_message(self.handle_message)
        
        if self.client.connect():
            log.write("[bold green]Connected to server[/bold green]")
            self.client.start()
            
            self.set_interval(0.1, self.poll_messages)
        else:
            log.write("[bold red]Failed to connect to server[/bold red]")
    
    def handle_message(self, msg: dict) -> None:
        """Callback for incoming messages from the exchange."""
        log = self.query_one("#echo_panel", RichLog)
        
        msg_type = msg.get("type", "UNKNOWN")
        
        if msg_type == "HELLO_ACK":
            self.hello_ack_received = True
            status = msg.get("status", "unknown")
            client_id = msg.get("clientId", "?")
            log.write(f"[bold cyan]< HELLO_ACK[/bold cyan] |"
                      f" Status: {status} | Client ID: {client_id}")
            
        elif msg_type == "ORDER_ACK":
            status = msg.get("status", "unknown")
            order_id = msg.get("server_order_id", "?")
            price = msg.get("accepted_price", "?")
            latency = msg.get("latency", "?")
            if price == 0:
                log.write(f"[bold yellow]< ORDER_ACK[/bold yellow] |"
                          f" Status: {status} | Order ID: {order_id} | "
                          f"Latency: {latency}μs")
            else:
                log.write(f"[bold yellow]< ORDER_ACK[/bold yellow] |"
                          f" Status: {status} | Order ID: {order_id} |"
                           f" Price: {price} | Latency: {latency}μs")
            
        elif msg_type == "TRADE":
            trade_id = msg.get("trade_id", "?")
            price = msg.get("price", "?")
            qty = msg.get("quantity", "?")
            log.write(f"[bold magenta]< TRADE[/bold magenta] |"
                      f" Trade ID: {trade_id} | Price: {price} | Qty: {qty}")
            
        elif msg_type == "CANCEL_ACK":
            status = msg.get("status", "unknown")
            order_id = msg.get("server_order_id", "?")
            log.write(f"[bold red]< CANCEL_ACK[/bold red] "
                      f"| Status: {status} | Order ID: {order_id}")
        
        elif msg_type == "MODIFY_ACK":
            status = msg.get("status", "?")
            old_order_id = msg.get("old_server_order_id", "?")
            new_order_id = msg.get("new_server_order_id", "?")
            log.write(f"[bold blue]< MODIFY_ACK[/bold blue] "
                      f"| Status: {status} | Old Order ID: {old_order_id} "
                      f"| New Server ID: {new_order_id}")
            
        elif msg_type == "LOGOUT_ACK":
            status = msg.get("status", "unknown")
            log.write(f"[bold]< LOGOUT_ACK[/bold] | Status: {status}")
            
        elif msg_type == "SESSION_TIMEOUT":
            log.write(f"[bold red]< SESSION_TIMEOUT[/bold red]")
            
        else:
            log.write(f"[dim]< {msg_type}[/dim] | {msg}")
    
    async def poll_messages(self) -> None:
        """Poll for messages from the client."""
        if self.client:
            try:
                msgs = await asyncio.to_thread(
                    self.client.wait_for_messages, 10
                )
            except Exception as e:
                log = self.query_one("#echo_panel", RichLog)
                log.write(f"[bold red]Error polling messages: {e}[/bold red]")
    
    def on_input_submitted(self, event: Input.Submitted) -> None:
        """Handle user commands."""
        log = self.query_one("#echo_panel", RichLog)
        command = event.value.strip().lower()
        
        if not command:
            event.input.value = ""
            return
        
        log.write(f"[bold white]> {event.value}[/bold white]")
        
        parts = command.split()
        cmd = parts[0]
        
        try:
            if cmd == "hello" or cmd == "login":
                self.client.send_hello() # type: ignore
                log.write("[dim]Sent HELLO[/dim]")

            elif cmd == "connect":
                if self.client.connect(): # type: ignore
                    log.write("[dim]Connecting to MiniExchange[/dim]")
                    self.client.start()
                else:
                    log.write("[bold red] Failed to Connect[/bold red]")
                
            elif cmd == "order" and len(parts) >= 4:
                side = parts[1]
                qty = int(parts[2])
                order_type = parts[3]
                
                is_buy = side == "buy"
                is_limit = order_type == "limit"
                
                if is_limit:
                    if len(parts) < 5:
                        log.write(
                            "[bold red]Error: Limit orders require a price[/bold red]"
                        )
                    else:
                        price = int(parts[4])
                        self.client.send_order(qty, price, is_buy, is_limit) # type: ignore
                        log.write(
                            f"[dim]Sent ORDER: {qty} @ {price} ({side}, {order_type})[/dim]"
                        )
                elif order_type == "market":
                    if len(parts) >= 5:
                        log.write(
                            "[bold red]Error: Market orders cannot have a price[/bold red]"
                        )
                    else:
                        price = 0
                        self.client.send_order(qty, price, is_buy, is_limit) # type: ignore
                        log.write(f"[dim]Sent ORDER: {qty} ({side}, {order_type})[/dim]")
                else:
                    log.write("[bold red]Error: Unknown order type[/bold red]")
                
            elif cmd == "cancel" and len(parts) >= 2:
                order_id = int(parts[1])
                self.client.send_cancel(order_id) # type: ignore
                log.write(f"[dim]Sent CANCEL for order {order_id}[/dim]")
            
            elif (cmd == "mod" or cmd == "modify") and len(parts) == 4:
                order_id = int(parts[1])
                newQty = int(parts[2])
                newPrice = int(parts[3])
                self.client.send_modify(order_id, newQty, newPrice) # type: ignore

            elif cmd == "logout":
                self.client.send_logout() # type: ignore
                
            elif cmd == "quit" or cmd == "exit" or cmd == "q":
                self.exit()
                
            elif cmd == "help":
                log.write("[bold]Available commands:[/bold]")
                log.write("  hello | login - Send HELLO message")
                log.write("  logout - Send LOGOUT message")
                log.write("  order <buy|sell> <qty> <limit|market> [price] - Send order")
                log.write("    Examples: order sell 100 limit 100")
                log.write("              order buy 50 market")
                log.write("  cancel <order_id> - Cancel order")
                log.write("  mod | modify <order_id> <new qty> <new price> - Modify order")
                log.write("  quit - Exit application")
                
            else:
                log.write(f"[bold red]Unknown command: "
                          f"{cmd}[/bold red] (type 'help' for commands)")
                
        except ValueError as e:
            log.write(f"[bold red]Invalid arguments: {e}[/bold red]")
        except Exception as e:
            log.write(f"[bold red]Error: {e}[/bold red]")
        
        event.input.value = ""
    
    def on_unmount(self) -> None:
        """Clean up when app closes."""
        if self.client:
            self.client.stop()


if __name__ == "__main__":
    app = MiniExchangeClient()
    app.run()