import sys
import time

sys.path.append("build-debug/py")

import miniexchange_client # type: ignore


def handle_message(msg):
    print("Got message:", msg)

if __name__ == "__main__":
    hmac_key = [0x11] * 32
    api_key = [0x22] * 16


    client = miniexchange_client.MiniExchangeClient(hmac_key, api_key)
    client.on_message(handle_message)
    client.connect()
    client.start()

    client.send_hello()
    client.send_order(100, 50, True, True)
    time.sleep(1)
    client.stop()
