from src.api.api import ExchangeAPI
from src.auth.session_manager import SessionManager, VSD
from src.feeds.events import EventBus


event_bus = EventBus()
session = SessionManager(user_db=VSD)
exchange = ExchangeAPI(event_bus=event_bus, session_manager=session)

login_request = {
    "type": "login",
    "payload": {
        "username": "testuser",
        "password": "test"
    }
}

result = exchange.handle_request(login_request)
print(result)
token = result.get("token")
logout_request = {
    "type": "logout",
    "payload": {
        "token": token
    }
}

logout = exchange.handle_request(logout_request)

print(logout)

event_bus.shutdown()
